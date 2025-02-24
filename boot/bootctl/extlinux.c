// SPDX-License-Identifier: GPL-2.0+
/*
 * Simple control of which display/output to use while booting
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#include <bootctl.h>
#include <bootmeth.h>
#include <bootstd.h>
#include <dm.h>
#include <log.h>
#include "oslist.h"

static int extlinux_bind(struct udevice *dev)
{
	struct bootctl_uc_plat *ucp = dev_get_uclass_plat(dev);

	ucp->type = BOOTCTLT_OSLIST;
	ucp->desc = "Provides OSes to boot";

	return 0;
}

static int extlinux_first(struct udevice *dev, struct oslist_iter *iter,
			  struct osinfo *info)
{
	int flags;

	bootstd_clear_glob();
	LOGR("eso", bootmeth_set_order("extlinux"));
	flags = BOOTFLOWIF_HUNT;
	LOGR("esf", bootflow_scan_first(NULL, NULL, &iter->bf_iter, flags,
					&info->bflow));

	return 0;
}

static int extlinux_next(struct udevice *dev, struct oslist_iter *iter,
			 struct osinfo *info)
{
	LOGR("esn", bootflow_scan_next(&iter->bf_iter, &info->bflow));

	return 0;
}

static struct bc_oslist_ops ops = {
	.first	= extlinux_first,
	.next	= extlinux_next,
};

static const struct udevice_id extlinux_ids[] = {
	{ .compatible = "bootctl,extlinux-oslist" },
	{ .compatible = "bootctl,os-list" },
	{ }
};

U_BOOT_DRIVER(extlinux) = {
	.name		= "extlinux",
	.id		= UCLASS_BOOTCTL,
	.of_match	= extlinux_ids,
	.bind		= extlinux_bind,
	.ops		= &ops,
};
