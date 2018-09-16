/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

/* Implementation of firmware storage access interface for SPI */

#include <common.h>
#include <dm.h>
#include <fdtdec.h>
#include <malloc.h>
#include <spi_flash.h>
#include <cros/cros_common.h>
#include <cros/cros_ofnode.h>
#include <cros/fwstore.h>

DECLARE_GLOBAL_DATA_PTR;

struct fwstore_spi_priv {
	struct udevice *spi_flash_dev;
};

/*
 * Check the right-exclusive range [offset:offset+*count_ptr), and adjust
 * value pointed by <count_ptr> to form a valid range when needed.
 *
 * Return 0 if it is possible to form a valid range. and non-zero if not.
 */
static int border_check(struct udevice *spi_flash_dev, uint32_t offset,
			uint32_t count)
{
	struct spi_flash *flash = dev_get_uclass_priv(spi_flash_dev);
	uint32_t max_offset = offset + count;

	if (offset >= flash->size) {
		VB2_DEBUG("at EOF: offset=%x, size=%x\n", offset, flash->size);
		return -ESPIPE;
	}

	/* max_offset will be less than offset iff overflow occurred. */
	if (max_offset < offset || max_offset > flash->size) {
		VB2_DEBUG("exceed range offset=%x, max_offset=%x, flash->size=%x\n",
			  offset, max_offset, flash->size);
		return -ERANGE;
	}

	return 0;
}

static int fwstore_spi_read(struct udevice *dev, ulong offset, ulong count,
			    void *buf)
{
	struct fwstore_spi_priv *priv = dev_get_priv(dev);
	int ret;

	ret = border_check(priv->spi_flash_dev, offset, count);
	if (ret)
		return ret;

	ret = spi_flash_read_dm(priv->spi_flash_dev, offset, count, buf);
	if (ret) {
		VB2_DEBUG("SPI read fail (count=%d, ret=%d)\n", count, ret);
		return ret;
	}

	return 0;
}

/*
 * FIXME: It is a reasonable assumption that sector size = 4096 bytes.
 * Nevertheless, comparing to coding this magic number here, there should be a
 * better way (maybe rewrite driver interface?) to expose this parameter from
 * eeprom driver.
 */
#define SECTOR_SIZE 0x1000

/*
 * Align the right-exclusive range [*offset_ptr:*offset_ptr+*length_ptr) with
 * SECTOR_SIZE.
 * After alignment adjustment, both offset and length will be multiple of
 * SECTOR_SIZE, and will be larger than or equal to the original range.
 */
static void align_to_sector(uint32_t *offset_ptr, uint32_t *length_ptr)
{
	VB2_DEBUG("before adjustment\n");
	VB2_DEBUG("offset: 0x%x\n", *offset_ptr);
	VB2_DEBUG("length: 0x%x\n", *length_ptr);

	/* Adjust if offset is not multiple of SECTOR_SIZE */
	if (*offset_ptr & (SECTOR_SIZE - 1ul)) {
		*offset_ptr &= ~(SECTOR_SIZE - 1ul);
	}

	/* Adjust if length is not multiple of SECTOR_SIZE */
	if (*length_ptr & (SECTOR_SIZE - 1ul)) {
		*length_ptr &= ~(SECTOR_SIZE - 1ul);
		*length_ptr += SECTOR_SIZE;
	}

	VB2_DEBUG("after adjustment\n");
	VB2_DEBUG("offset: 0x%x\n", *offset_ptr);
	VB2_DEBUG("length: 0x%x\n", *length_ptr);
}

static int fwstore_spi_write(struct udevice *dev, ulong offset,
			     ulong count, void *buf)
{
	struct fwstore_spi_priv *priv = dev_get_priv(dev);
	uint8_t static_buf[SECTOR_SIZE];
	uint8_t *backup_buf;
	uint32_t k, n;
	int status, ret = -1;

	/* We will erase <n> bytes starting from <k> */
	k = offset;
	n = count;
	align_to_sector(&k, &n);

	VB2_DEBUG("offset:          0x%08x\n", offset);
	VB2_DEBUG("adjusted offset: 0x%08x\n", k);
	if (k > offset) {
		VB2_DEBUG("align incorrect: %08x > %08x\n", k, offset);
		return -1;
	}

	if (border_check(priv->spi_flash_dev, k, n))
		return -1;

	backup_buf = n > sizeof(static_buf) ? malloc(n) : static_buf;

	if ((status = spi_flash_read_dm(priv->spi_flash_dev, k, n, backup_buf))) {
		VB2_DEBUG("cannot backup data: %d\n", status);
		goto EXIT;
	}

	if ((status = spi_flash_erase_dm(priv->spi_flash_dev, k, n))) {
		VB2_DEBUG("SPI erase fail: %d\n", status);
		goto EXIT;
	}

	/* combine data we want to write and backup data */
	memcpy(backup_buf + (offset - k), buf, count);

	if (spi_flash_write_dm(priv->spi_flash_dev, k, n, backup_buf)) {
		VB2_DEBUG("SPI write fail\n");
		goto EXIT;
	}

	ret = 0;

EXIT:
	if (backup_buf != static_buf)
		free(backup_buf);

	return ret;
}

static int fwstore_spi_sw_wp_enabled(struct udevice *dev)
{
	uint8_t yes_it_is = 0;
	int r = 0;

// 	r = spi_flash_read_sw_wp_status(file->context, &yes_it_is);
	if (r) {
		VB2_DEBUG("spi_flash_read_sw_wp_status() failed: %d\n", r);
		return 0;
	}

	VB2_DEBUG("flash SW WP is %d\n", yes_it_is);
	return yes_it_is;
}

int fwstore_spi_probe(struct udevice *dev)
{
	struct fwstore_spi_priv *priv = dev_get_priv(dev);
	struct ofnode_phandle_args args;
	int ret;

	log(LOGC_DM, LOGL_DEBUG, "init %s\n", dev->name);
	ret = dev_read_phandle_with_args(dev, "firmware-storage", NULL, 0, 0,
					 &args);
	if (ret < 0) {
		VB2_DEBUG("fail to look up phandle for device %s\n", dev->name);
		return ret;
	}

	ret = uclass_get_device_by_ofnode(UCLASS_SPI_FLASH, args.node,
					  &priv->spi_flash_dev);
	if (ret) {
		VB2_DEBUG("fail to init SPI flash at %s: %s: ret=%d\n",
			  dev->name, ofnode_get_name(args.node), ret);
		return ret;
	}

	return 0;
}

static const struct cros_fwstore_ops fwstore_spi_ops = {
	.read		= fwstore_spi_read,
	.write		= fwstore_spi_write,
	.sw_wp_enabled	= fwstore_spi_sw_wp_enabled,
};

static struct udevice_id fwstore_spi_ids[] = {
	{ .compatible = "cros,fwstore-spi" },
	{ },
};

U_BOOT_DRIVER(fwstore_spi) = {
	.name	= "fwstore_spi",
	.id	= UCLASS_CROS_FWSTORE,
	.of_match = fwstore_spi_ids,
	.ops	= &fwstore_spi_ops,
	.probe	= fwstore_spi_probe,
	.priv_auto_alloc_size = sizeof(struct fwstore_spi_priv),
};
