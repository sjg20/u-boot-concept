/*
 * Copyright (C) 2014 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <pch.h>

static int baytrail_pch_get_sbase(struct udevice *dev, ulong *sbasep)
{
	uint32_t sbase_addr;

	dm_pci_read_config32(dev, 0x54, &sbase_addr);
	*sbasep = sbase_addr & 0xfffffe00;

	return 0;
}

static int baytrail_pch_get_version(struct udevice *dev)
{
	return 9;
}

static const struct pch_ops baytrail_pch_ops = {
	.get_sbase	= baytrail_pch_get_sbase,
	.get_version	= baytrail_pch_get_version,
};

static const struct udevice_id baytrailpch_ids[] = {
	{ .compatible = "intel,pch9" },
	{ }
};

U_BOOT_DRIVER(pch9_drv) = {
	.name		= "intel-pch",
	.id		= UCLASS_PCH,
	.of_match	= baytrailpch_ids,
	.ops		= &baytrail_pch_ops,
};
