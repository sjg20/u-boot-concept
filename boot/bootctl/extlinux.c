// SPDX-License-Identifier: GPL-2.0+
/*
 * Access to extlinux.conf files containing OS information
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#define LOG_CATEGORY	UCLASS_BOOTCTL

#include <bootctl.h>
#include <bootmeth.h>
#include <bootstd.h>
#include <dm.h>
#include <log.h>
#include <bootctl/oslist.h>

static int extlinux_bind(struct udevice *dev)
{
	struct bootctl_uc_plat *ucp = dev_get_uclass_plat(dev);

	ucp->desc = "Provides OSes to boot";

	return 0;
}

static int extlinux_next(struct udevice *dev, struct oslist_iter *iter,
			 struct osinfo *info)
{
	int ret;

	if (!iter->active) {
		bootstd_clear_glob();
		iter->active = true;
		ret = bootmeth_set_order("extlinux");
		if (ret)
			return log_msg_ret("eso", ret);
		ret = bootflow_scan_first(NULL, NULL, &iter->bf_iter,
					  BOOTFLOWIF_HUNT, &info->bflow);
		if (ret)
			return log_msg_ret("esf", ret);
	} else {
		ret = bootflow_scan_next(&iter->bf_iter, &info->bflow);
		if (ret) {
			iter->active = false;
			if (ret)
				return log_msg_ret("esn", ret);
		}
	}

	return 0;
}

static struct bc_oslist_ops ops = {
	.next	= extlinux_next,
};

static const struct udevice_id extlinux_ids[] = {
	{ .compatible = "bootctl,extlinux-oslist" },
	{ .compatible = "bootctl,os-list" },
	{ }
};

U_BOOT_DRIVER(extlinux) = {
	.name		= "extlinux",
	.id		= UCLASS_BOOTCTL_OSLIST,
	.of_match	= extlinux_ids,
	.bind		= extlinux_bind,
	.ops		= &ops,
};
