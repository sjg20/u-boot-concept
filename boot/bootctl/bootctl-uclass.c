// SPDX-License-Identifier: GPL-2.0+
/*
 * Uclass implementation for boot schema
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#include <bootctl.h>
#include <dm.h>

static int bootctl_drv_bind(struct udevice *dev)
{
	struct bootctl_uc_plat *ucp = dev_get_uclass_plat(dev);

	ucp->desc = "Core bootctl";

	return 0;
}

static const struct udevice_id bootctl_ids[] = {
	{ .compatible = "bootctl" },
	{ }
};

U_BOOT_DRIVER(bootctl_drv) = {
	.id		= UCLASS_BOOTCTL,
	.name		= "bootctl_drv",
	.of_match	= bootctl_ids,
	.bind		= bootctl_drv_bind,
};

UCLASS_DRIVER(bootctrl) = {
	.id		= UCLASS_BOOTCTL,
	.name		= "bootctrl",
#if CONFIG_IS_ENABLED(OF_REAL)
	.post_bind	= dm_scan_fdt_dev,
#endif
	.per_device_plat_auto	= sizeof(struct bootctl_uc_plat),
};
