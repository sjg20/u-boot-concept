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
#include "state.h"

static int simple_state_bind(struct udevice *dev)
{
	struct bootctl_uc_plat *ucp = dev_get_uclass_plat(dev);

	ucp->desc = "Stores state information about booting";

	return 0;
}

static int simple_state_first(struct udevice *dev)
{
	return 0;
}

static struct bc_state_ops ops = {
	.first	= simple_state_first,
};

static const struct udevice_id simple_state_ids[] = {
	{ .compatible = "bootctl,simple-state" },
	{ .compatible = "bootctl,state" },
	{ }
};

U_BOOT_DRIVER(simple_state) = {
	.name		= "simple_state",
	.id		= UCLASS_BOOTCTL_STATE,
	.of_match	= simple_state_ids,
	.bind		= simple_state_bind,
	.ops		= &ops,
};
