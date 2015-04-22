/*
 * Copyright (c) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <asm/cpu.h>
#include <dm/lists.h>
#include <dm/root.h>

static int cpu_bus_post_bind(struct udevice *dev)
{
	struct cpu_platdata *plat = dev_get_parent_platdata(dev);

	plat->apic_id = fdtdec_get_int(gd->fdt_blob, dev->of_offset,
				       "intel,apic-id", -1);

	return 0;
}

U_BOOT_DRIVER(cpu_bus) = {
	.name	= "cpu_bus",
	.id	= UCLASS_SIMPLE_BUS,
	.child_post_bind	= cpu_bus_post_bind,
	.per_child_platdata_auto_alloc_size = sizeof(struct cpu_platdata),
};

static int uclass_cpu_init(struct uclass *uc)
{
	struct udevice *dev;
	int node;
	int ret;

	node = fdt_path_offset(gd->fdt_blob, "/cpus");
	if (node < 0)
		return 0;

	ret = device_bind_driver_to_node(dm_root(), "cpu_bus", "cpus", node,
					 &dev);

	return ret;
}

UCLASS_DRIVER(cpu) = {
	.id		= UCLASS_CPU,
	.name		= "x86_cpu",
	.flags		= DM_UC_FLAG_SEQ_ALIAS,
	.init		= uclass_cpu_init,
};
