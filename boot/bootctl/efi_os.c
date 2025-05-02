// SPDX-License-Identifier: GPL-2.0+
/*
 * Access to EFI files containing an 'opaque' OS
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

static int efifile_bind(struct udevice *dev)
{
	struct bootctl_uc_plat *ucp = dev_get_uclass_plat(dev);

	ucp->desc = "Provides OSes to boot";

	return 0;
}

static int efifile_next(struct udevice *dev, struct oslist_iter *iter,
			struct osinfo *info)
{
	int ret;

	if (!iter->active) {
		bootstd_clear_glob();
		iter->active = true;
		ret = bootmeth_set_order("efi");
		if (ret)
			return log_msg_ret("esf", ret);
		ret = bootflow_scan_first(NULL, NULL, &iter->bf_iter,
					  BOOTFLOWIF_HUNT, &info->bflow);
		if (ret)
			return log_msg_ret("Esf", ret);
	} else {
		ret = bootflow_scan_next(&iter->bf_iter, &info->bflow);
		if (ret) {
			iter->active = false;
			return log_msg_ret("Esn", ret);
		}
	}

	return 0;
}

static struct bc_oslist_ops ops = {
	.next	= efifile_next,
};

static const struct udevice_id efifile_ids[] = {
	{ .compatible = "bootctl,efifile-oslist" },
	{ .compatible = "bootctl,os-list" },
	{ }
};

U_BOOT_DRIVER(efifile) = {
	.name		= "efifile",
	.id		= UCLASS_BOOTCTL_OSLIST,
	.of_match	= efifile_ids,
	.bind		= efifile_bind,
	.ops		= &ops,
};
