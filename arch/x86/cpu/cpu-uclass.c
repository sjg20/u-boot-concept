/*
 * Copyright (c) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <dm/lists.h>

static int uclass_cpu_init(struct uclass *uc)
{
	struct udevice *dev;
	int node;
	int ret;

	node = fdt_path_offset(gd->fdt_blob, "/cpus");
	if (node < 0)
		return 0;

	ret = device_bind_driver(gd->dm_root, "generic_simple_bus", "cpus",
				 &dev);

	return ret;
}

UCLASS_DRIVER(cpu) = {
	.id		= UCLASS_CPU,
	.name		= "x86_cpu",
	.flags		= DM_UC_FLAG_SEQ_ALIAS,
	.init		= uclass_cpu_init,
};
