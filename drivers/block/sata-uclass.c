/*
 * SATA device U-Class driver
 *
 * (C) Copyright 2016
 *     Texas Instruments Incorporated, <www.ti.com>
 *
 * Author: Mugunthan V N <mugunthanvnm@ti.com>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <dm/device-internal.h>
#include <errno.h>
#include <scsi.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * struct mmc_uclass_priv - Holds information about a device used by the uclass
 */
struct sata_uclass_priv {
	struct block_dev_desc_t *block_dev;
};

int scsi_get_device(int index, struct udevice **devp)
{
	struct udevice *dev;
	int ret;

	ret = uclass_find_device(UCLASS_SATA, index, &dev);
	if (ret || !dev) {
		printf("%d device not found\n", index);
		return ret;
	}

	ret = device_probe(dev);
	if (ret) {
		error("device probe error\n");
		return ret;
	}

	*devp = dev;

	return ret;
}

void scsi_init(void)
{
	struct udevice *dev;
	int ret;

	ret = scsi_get_device(0, &dev);
	if (ret || !dev) {
		error("scsi device not found\n");
		return;
	}

	scsi_scan(1);
}

UCLASS_DRIVER(sata) = {
	.id		= UCLASS_SATA,
	.name		= "sata",
	.flags		= DM_UC_FLAG_SEQ_ALIAS,
	.per_device_auto_alloc_size = sizeof(struct sata_uclass_priv),
};
