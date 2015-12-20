/*
 * Copyright (C) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <errno.h>
#include <dm/lists.h>
#include <dm/root.h>

DECLARE_GLOBAL_DATA_PTR;

ulong clk_get_rate(struct udevice *dev)
{
	struct clk_ops *ops = clk_get_ops(dev);

	if (!ops->get_rate)
		return -ENOSYS;

	return ops->get_rate(dev);
}

ulong clk_set_rate(struct udevice *dev, ulong rate)
{
	struct clk_ops *ops = clk_get_ops(dev);

	if (!ops->set_rate)
		return -ENOSYS;

	return ops->set_rate(dev, rate);
}

ulong clk_get_periph_rate(struct udevice *dev, int periph)
{
	struct clk_ops *ops = clk_get_ops(dev);

	if (!ops->get_periph_rate)
		return -ENOSYS;

	return ops->get_periph_rate(dev, periph);
}

ulong clk_set_periph_rate(struct udevice *dev, int periph, ulong rate)
{
	struct clk_ops *ops = clk_get_ops(dev);

	if (!ops->set_periph_rate)
		return -ENOSYS;

	return ops->set_periph_rate(dev, periph, rate);
}

int clk_get_id(struct udevice *dev, int args_count, uint32_t *args)
{
	struct clk_ops *ops = clk_get_ops(dev);

	if (!ops->get_id)
		return -ENOSYS;

	return ops->get_id(dev, args_count, args);
}

int clk_get_by_index(struct udevice *dev, int index, struct udevice **clk_devp,
		     int *periphp)
{
	struct fdtdec_phandle_args args;
	int ret;

	ret = fdtdec_parse_phandle_with_args(gd->fdt_blob, dev->of_offset,
					     "clocks", "#clock-cells", 0, index,
					     &args);
	if (ret) {
		debug("%s: fdtdec_parse_phandle_with_args failed: err=%d\n",
		      __func__, ret);
		return ret;
	}

	ret = uclass_get_device_by_of_offset(UCLASS_CLK, args.node, clk_devp);
	if (ret) {
		debug("%s: uclass_get_device_by_of_offset failed: err=%d\n",
		      __func__, ret);
		return ret;
	}
	*periphp = args.args_count > 0 ? args.args[0] : -1;

	return 0;
}

UCLASS_DRIVER(clk) = {
	.id		= UCLASS_CLK,
	.name		= "clk",
};
