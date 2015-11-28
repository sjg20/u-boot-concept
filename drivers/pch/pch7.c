/*
 * Copyright (C) 2014 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <pch.h>

static int queensbay_pch_get_version(struct udevice *dev)
{
	return 7;
}

static const struct pch_ops queensbay_pch9_ops = {
	.get_version	= queensbay_pch_get_version,
};

static const struct udevice_id queensbay_pch_ids[] = {
	{ .compatible = "intel,pch7" },
	{ }
};

U_BOOT_DRIVER(queensbay_drv) = {
	.name		= "intel-pch",
	.id		= UCLASS_PCH,
	.of_match	= queensbay_pch_ids,
	.ops		= &queensbay_pch9_ops,
};
