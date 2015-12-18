/*
 * Copyright (C) 2015 Masahiro Yamada <yamada.masahiro@socionext.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <clk.h>
#include <dm/device.h>

DECLARE_GLOBAL_DATA_PTR;

struct clk_fixed_rate {
	unsigned long fixed_rate;
};

#define to_clk_fixed_rate(dev)	((struct clk_fixed_rate *)dev_get_priv(dev))

static ulong clk_fixed_get_rate(struct udevice *dev)
{
	return to_clk_fixed_rate(dev)->fixed_rate;
}

static ulong clk_fixed_get_periph_rate(struct udevice *dev, int ignored)
{
	return to_clk_fixed_rate(dev)->fixed_rate;
}

const struct clk_ops clk_fixed_rate_ops = {
	.get_rate = clk_fixed_get_rate,
	.get_periph_rate = clk_fixed_get_periph_rate,
	.get_id = clk_get_id_simple,
};

static int clk_fixed_rate_probe(struct udevice *dev)
{
	to_clk_fixed_rate(dev)->fixed_rate =
				fdtdec_get_int(gd->fdt_blob, dev->of_offset,
					       "clock-frequency", 0);

	return 0;
}

static const struct udevice_id clk_fixed_rate_match[] = {
	{
		.compatible = "fixed-clock",
	},
	{ /* sentinel */ }
};

U_BOOT_DRIVER(clk_fixed_rate) = {
	.name = "Fixed Rate Clock",
	.id = UCLASS_CLK,
	.of_match = clk_fixed_rate_match,
	.probe = clk_fixed_rate_probe,
	.priv_auto_alloc_size = sizeof(struct clk_fixed_rate),
	.ops = &clk_fixed_rate_ops,
};
