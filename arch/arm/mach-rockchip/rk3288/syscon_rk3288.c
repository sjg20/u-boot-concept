// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <syscon.h>
#include <asm/arch-rockchip/clock.h>

static const struct udevice_id rk3288_syscon_ids[] = {
	{ .compatible = "rockchip,rk3288-noc", .data = ROCKCHIP_SYSCON_NOC },
	{ .compatible = "rockchip,rk3288-grf", .data = ROCKCHIP_SYSCON_GRF },
	{ .compatible = "rockchip,rk3288-sgrf", .data = ROCKCHIP_SYSCON_SGRF },
	{ .compatible = "rockchip,rk3288-pmu", .data = ROCKCHIP_SYSCON_PMU },
	{ }
};

U_BOOT_DRIVER(syscon_rk3288) = {
	.name = "rk3288_syscon",
	.id = UCLASS_SYSCON,
	.of_match = rk3288_syscon_ids,
};

#if CONFIG_IS_ENABLED(OF_PLATDATA)
static int rk3288_syscon_bind_of_platdata(struct udevice *dev)
{
	dev->driver_data = dev->driver->of_match->data;
	debug("syscon: %s %d\n", dev->name, (uint)dev->driver_data);

	return 0;
}

static const struct udevice_id rk3288_syscon_ids_noc[] = {
	{ .compatible = "rockchip,rk3288-noc", .data = ROCKCHIP_SYSCON_NOC },
	{ }
};

U_BOOT_DRIVER(rockchip_rk3288_noc) = {
	.name = "rockchip_rk3288_noc",
	.id = UCLASS_SYSCON,
	.of_match = rk3288_syscon_ids_noc,
	.bind = rk3288_syscon_bind_of_platdata,
};

static const struct udevice_id rk3288_syscon_ids_grf[] = {
	{ .compatible = "rockchip,rk3288-grf", .data = ROCKCHIP_SYSCON_GRF },
	{ }
};

U_BOOT_DRIVER(rockchip_rk3288_grf) = {
	.name = "rockchip_rk3288_grf",
	.id = UCLASS_SYSCON,
	.of_match = rk3288_syscon_ids_grf,
	.bind = rk3288_syscon_bind_of_platdata,
};

static const struct udevice_id rk3288_syscon_ids_sgrf[] = {
	{ .compatible = "rockchip,rk3288-sgrf", .data = ROCKCHIP_SYSCON_SGRF },
	{ }
};

U_BOOT_DRIVER(rockchip_rk3288_sgrf) = {
	.name = "rockchip_rk3288_sgrf",
	.id = UCLASS_SYSCON,
	.of_match = rk3288_syscon_ids_sgrf,
	.bind = rk3288_syscon_bind_of_platdata,
};

static const struct udevice_id rk3288_syscon_ids_pmu[] = {
	{ .compatible = "rockchip,rk3288-pmu", .data = ROCKCHIP_SYSCON_PMU },
	{ }
};

U_BOOT_DRIVER(rockchip_rk3288_pmu) = {
	.name = "rockchip_rk3288_pmu",
	.id = UCLASS_SYSCON,
	.of_match = rk3288_syscon_ids_pmu,
	.bind = rk3288_syscon_bind_of_platdata,
};
#endif
