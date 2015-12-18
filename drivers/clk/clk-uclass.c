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

UCLASS_DRIVER(clk) = {
	.id		= UCLASS_CLK,
	.name		= "clk",
};
