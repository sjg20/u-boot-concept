// SPDX-License-Identifier: GPL-2.0+
/*
 * Intel 'Fast SPI' support
 *
 * Copyright 2019 Google LLC
 */

#include <common.h>
#include <dm.h>
#include <pci.h>
#include <spi_flash.h>
#include <spl.h>
#include <asm/io.h>
#include <asm/pci.h>
#include <asm/arch/fast_spi.h>
#include <asm/arch/pch.h>

struct fast_spi_priv {
	struct fast_spi_regs *regs;
	uint page_size;
	ulong mmio_base;
	uint flash_size;
	uint map_offset;
	ulong map_base;
	size_t map_size;
};

enum {
	/* Erase size options */
	ERASE_SIZE_SM	= 4 << 10,
	ERASE_SIZE_LG	= 64 << 10,
};

/*
 * The hardware datasheet is not clear on what HORD values actually do. It
 * seems that HORD_SFDP provides access to the first 8 bytes of the SFDP, which
 * is the signature and revision fields. HORD_JEDEC provides access to the
 * actual flash parameters, and is most likely what you want to use when
 * probing the flash from software.
 * It's okay to rely on SFDP, since the SPI flash controller requires an SFDP
 * 1.5 or newer compliant FAST_SPI flash chip.
 * NOTE: Due to the register layout of the hardware, all accesses will be
 * aligned to a 4 byte boundary.
 */
static u32 read_sfdp_param(struct fast_spi_priv *priv, uint sfdp_reg)
{
	u32 ptinx_index = sfdp_reg & SPIBAR_PTINX_IDX_MASK;

	writel(ptinx_index | SPIBAR_PTINX_HORD_JEDEC, &priv->regs->ptinx);

	return readl(&priv->regs->ptdata);
}

/* Fill FDATAn FIFO in preparation for a write transaction */
static void fill_xfer_fifo(struct fast_spi_priv *priv, const void *data,
			   size_t len)
{
	memcpy(priv->regs->fdata, data, len);
}

/* Drain FDATAn FIFO after a read transaction populates data */
static void drain_xfer_fifo(struct fast_spi_priv *priv, void *dest, size_t len)
{
	memcpy(dest, priv->regs->fdata, len);
}

/* Fire up a transfer using the hardware sequencer */
static void start_hwseq_xfer(struct fast_spi_priv *priv, u32 hsfsts_cycle,
			     u32 offset, size_t len)
{
	/* Make sure all W1C status bits get cleared */
	u32 hsfsts = SPIBAR_HSFSTS_W1C_BITS;

	/* Set up transaction parameters */
	hsfsts |= hsfsts_cycle & SPIBAR_HSFSTS_FCYCLE_MASK;
	hsfsts |= SPIBAR_HSFSTS_FDBC(len - 1);

	writel(offset, &priv->regs->faddr);
	writel(hsfsts | SPIBAR_HSFSTS_FGO, &priv->regs->hsfsts_ctl);
}

static int wait_for_hwseq_xfer(struct fast_spi_priv *priv, u32 offset)
{
	ulong start;
	u32 hsfsts;

	start = get_timer(0);
	do {
		hsfsts = readl(&priv->regs->hsfsts_ctl);

		if (hsfsts & SPIBAR_HSFSTS_FCERR) {
			debug("SPI transaction error at offset %x HSFSTS = %08x\n",
			      offset, hsfsts);
			return -EIO;
		}

		if (hsfsts & SPIBAR_HSFSTS_FDONE)
			return 0;
	} while ((int)get_timer(start) < SPIBAR_HWSEQ_XFER_TIMEOUT_MS);

	debug("SPI transaction timeout at offset %x HSFSTS = %08x, timer %d\n",
	      offset, hsfsts, (uint)get_timer(start));
	return -ETIMEDOUT;
}

/* Execute FAST_SPI flash transfer. This is a blocking call */
static int exec_sync_hwseq_xfer(struct fast_spi_priv *priv,
				u32 hsfsts_cycle, u32 offset,
				size_t len)
{
	start_hwseq_xfer(priv, hsfsts_cycle, offset, len);

	return wait_for_hwseq_xfer(priv, offset);
}

/*
 * Ensure read/write xfer len is not greater than SPIBAR_FDATA_FIFO_SIZE and
 * that the operation does not cross page boundary.
 */
static size_t get_xfer_len(const struct fast_spi_priv *priv, u32 offset,
			   size_t len)
{
	size_t xfer_len = min(len, (size_t)SPIBAR_FDATA_FIFO_SIZE);
	size_t bytes_left = ALIGN(offset, priv->page_size) - offset;

	if (bytes_left)
		xfer_len = min(xfer_len, bytes_left);

	return xfer_len;
}

static int fast_spi_flash_erase(struct udevice *dev, u32 offset, size_t len)
{
	struct fast_spi_priv *priv = dev_get_priv(dev);
	int ret;
	size_t erase_size;
	u32 erase_cycle;

	if (!IS_ALIGNED(offset, ERASE_SIZE_SM) ||
	    !IS_ALIGNED(len, ERASE_SIZE_SM)) {
		debug("SPI erase region not sector-aligned\n");
		return -EINVAL;
	}

	while (len) {
		if (IS_ALIGNED(offset, ERASE_SIZE_LG) && len >= ERASE_SIZE_LG) {
			erase_size = ERASE_SIZE_LG;
			erase_cycle = SPIBAR_HSFSTS_CYCLE_64K_ERASE;
		} else {
			erase_size = ERASE_SIZE_SM;
			erase_cycle = SPIBAR_HSFSTS_CYCLE_4K_ERASE;
		}
		debug("Erasing flash addr %x + %x\n", offset, (uint)erase_size);

		ret = exec_sync_hwseq_xfer(priv, erase_cycle, offset, 0);
		if (ret)
			return ret;

		offset += erase_size;
		len -= erase_size;
	}

	return 0;
}

static int fast_spi_read(struct udevice *dev, u32 offset, size_t len, void *buf)
{
	struct fast_spi_priv *priv = dev_get_priv(dev);

	debug("%s: read at offset %x\n", __func__, offset);
	while (len) {
		size_t xfer_len = get_xfer_len(priv, offset, len);
		int ret;

		ret = exec_sync_hwseq_xfer(priv, SPIBAR_HSFSTS_CYCLE_READ,
					   offset, xfer_len);
		if (ret)
			return ret;

		drain_xfer_fifo(priv, buf, xfer_len);

		offset += xfer_len;
		buf += xfer_len;
		len -= xfer_len;
	}

	return 0;
}

static int fast_spi_flash_write(struct udevice *dev, u32 addr, size_t len,
				const void *buf)
{
	struct fast_spi_priv *priv = dev_get_priv(dev);
	const u8 *data = buf;
	size_t xfer_len;
	int ret;

	while (len) {
		xfer_len = get_xfer_len(priv, addr, len);
		fill_xfer_fifo(priv, data, xfer_len);

		ret = exec_sync_hwseq_xfer(priv, SPIBAR_HSFSTS_CYCLE_WRITE,
					   addr, xfer_len);
		if (ret)
			return ret;

		addr += xfer_len;
		data += xfer_len;
		len -= xfer_len;
	}

	return 0;
}

static int fast_spi_get_mmap(struct udevice *dev, ulong *map_basep,
			     size_t *map_sizep, u32 *offsetp)
{
	struct fast_spi_priv *priv = dev_get_priv(dev);

	if (priv) {
		*map_basep = priv->map_base;
		*map_sizep = priv->map_size;
		*offsetp = priv->map_offset;
	} else {
		return fast_spi_get_bios_mmap(map_basep, map_sizep, offsetp);
	}

	return 0;
}

static int fast_spi_probe(struct udevice *dev)
{
	struct spi_flash *flash = dev_get_uclass_priv(dev);
	struct fast_spi_priv *priv = dev_get_priv(dev);
	u32 flash_bits;
	ulong bar, base;
	int bdf;

        /* TODO(sjg@chromium.org): Tidy this up when the device tree is ready */
#if 0 && CONFIG_IS_ENABLED(PCI)
	bdf = pci_get_devfn(dev);
	if (bdf < 0)
		return bdf;
#else
	bdf = PCH_DEV_SPI;
#endif
	pci_x86_read_config(NULL, bdf, PCI_BASE_ADDRESS_0, &bar, PCI_SIZE_32);

	priv->mmio_base = bar & PCI_BASE_ADDRESS_MEM_MASK;
	priv->regs = (struct fast_spi_regs *)priv->mmio_base;

	/*
	 * bytes = (bits + 1) / 8;
	 * But we need to do the addition in a way which doesn't overflow for
	 * 4 Gb devices (flash_bits == 0xffffffff).
	 */
	flash_bits = read_sfdp_param(priv, 0x04);
	flash->size = (flash_bits >> 3) + 1;

	/* Can erase both 4 KiB and 64 KiB chunks. Declare the smaller size */
	flash->sector_size = 4 << 10;
	flash->page_size = 256;

	base = fast_spi_get_bios_region(priv->regs, &priv->map_size);
	priv->map_base = (u32)-priv->map_size - base;
	priv->map_offset = base;

	debug("FAST SPI at %lx, size %x with mapping %x, size %x\n",
	      priv->mmio_base, flash->size, (uint)priv->map_base,
	      priv->map_size);

	return 0;
}

static const struct dm_spi_flash_ops fast_spi_ops = {
	.read = fast_spi_read,
	.write = fast_spi_flash_write,
	.erase = fast_spi_flash_erase,
	.get_mmap = fast_spi_get_mmap,
};

static const struct udevice_id fast_spi_ids[] = {
	{ .compatible = "intel,fast-spi" },
	{ }
};

U_BOOT_DRIVER(intel_fast_spi) = {
	.name		= "intel_fast_spi",
	.id		= UCLASS_SPI_FLASH,
	.of_match	= fast_spi_ids,
	.probe		= fast_spi_probe,
	.priv_auto_alloc_size = sizeof(struct fast_spi_priv),
	.ops		= &fast_spi_ops,
};
