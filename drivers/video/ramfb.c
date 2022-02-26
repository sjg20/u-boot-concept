// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2022 Alexander Graf
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <video.h>
#include <asm/global_data.h>
#include <efi_loader.h>
#include <qfw.h>


#define fourcc_code(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | \
				 ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
#define DRM_FORMAT_XRGB8888 fourcc_code('X', 'R', '2', '4')

#define DEFAULT_WIDTH	1280
#define DEFAULT_HEIGHT	900
#define DEFAULT_BPIX	VIDEO_BPP32
#define DEFAULT_FORMAT	VIDEO_X8R8G8B8

struct ramfb_cfg {
	uint64_t addr;
	uint32_t fourcc;
	uint32_t flags;
	uint32_t width;
	uint32_t height;
	uint32_t stride;
} __attribute__ ((__packed__));;

static int ramfb_probe(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	uint32_t selector;
	uint64_t base;
	uint64_t size;
	efi_status_t ret;
	struct fw_file *file;
	struct udevice *qfw;
	struct dm_qfw_ops *ops;
	struct qfw_dma dma = {};
	struct ramfb_cfg cfg = {
		.addr = 0,
		.fourcc = cpu_to_be32(DRM_FORMAT_XRGB8888),
		.flags = 0,
		.width = cpu_to_be32(DEFAULT_WIDTH),
		.height = cpu_to_be32(DEFAULT_HEIGHT),
		.stride = 0,
	};

	ret = qfw_get_dev(&qfw);
	if (ret)
		return -EPROBE_DEFER;

	ops = dm_qfw_get_ops(qfw);
	if (!ops)
		return -EPROBE_DEFER;

	file = qfw_find_file(qfw, "etc/ramfb");
	if (!file) {
		/* No ramfb available. At least we tried. */
		return -ENOENT;
	}

	size = DEFAULT_WIDTH * DEFAULT_HEIGHT * VNBYTES(DEFAULT_BPIX);
	ret = efi_allocate_pages(EFI_ALLOCATE_ANY_PAGES,
				 EFI_RESERVED_MEMORY_TYPE,
				 efi_size_in_pages(size), &base);
	if (ret != EFI_SUCCESS)
		return -ENOMEM;

	debug("%s: base=%llx, size=%llu\n", __func__, base, size);

	cfg.addr = cpu_to_be64(base);
	plat->base = base;
	plat->size = size;
	uc_priv->xsize = DEFAULT_WIDTH;
	uc_priv->ysize = DEFAULT_HEIGHT;
	uc_priv->bpix = DEFAULT_BPIX;
	uc_priv->format = DEFAULT_FORMAT;
	uc_priv->fb = (void*)base;
	uc_priv->fb_size = size;

	selector = be16_to_cpu(file->cfg.select);
	dma.length = cpu_to_be32(sizeof(cfg));
	dma.address = cpu_to_be64((uintptr_t)&cfg);
	dma.control = cpu_to_be32(FW_CFG_DMA_WRITE | FW_CFG_DMA_SELECT |
				  (selector << 16));

	barrier();

	/* Send a DMA write request which enables the screen */
	ops->read_entry_dma(qfw, &dma);

	return 0;
}

U_BOOT_DRIVER(ramfb) = {
	.name	= "ramfb",
	.id	= UCLASS_VIDEO,
	.probe	= ramfb_probe,
};
