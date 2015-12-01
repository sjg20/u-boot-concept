/*
 * Direct Memory Access U-Class driver
 *
 * (C) Copyright 2015
 *     Texas Instruments Incorporated, <www.ti.com>
 *
 * Author: Mugunthan V N <mugunthanvnm@ti.com>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <dma.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <errno.h>

DECLARE_GLOBAL_DATA_PTR;

int dma_memcpy(void *dst, void *src, size_t len)
{
	struct udevice *dev;
	const struct dma_ops *ops;
	int ret;

	for (ret = uclass_find_first_device(UCLASS_DMA, &dev); dev && !ret;
	     ret = uclass_find_next_device(&dev)) {
		struct dma_dev_priv *uc_priv = dev_get_uclass_priv(dev);

		if (uc_priv->supported & SUPPORTS_MEM_TO_MEM)
			break;
	}

	if (!dev) {
		printf("No DMA device found that supports mem to mem transfers\n");
		return -ENODEV;
	}

	ops = device_get_ops(dev);
	if (!ops->transfer)
		return -ENOSYS;

	/* Invalidate the area, so no writeback into the RAM races with DMA */
	invalidate_dcache_range((unsigned long)dst, (unsigned long)dst +
				roundup(len, ARCH_DMA_MINALIGN));

	return ops->transfer(dev, DMA_MEM_TO_MEM, dst, src, len);
}

int dma_post_bind(struct udevice *dev)
{
	struct udevice *devp;
	uclass_get_device_by_of_offset(UCLASS_DMA, dev->of_offset, &devp);
	return 0;
}

UCLASS_DRIVER(dma) = {
	.id		= UCLASS_DMA,
	.name		= "dma",
	.flags		= DM_UC_FLAG_SEQ_ALIAS,
	.per_device_auto_alloc_size = sizeof(struct dma_dev_priv),
	.post_bind	= dma_post_bind,
};
