/*
 * Copyright (c) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <pinctrl.h>
#include <dm/root.h>

DECLARE_GLOBAL_DATA_PTR;

int pinctrl_request(struct udevice *dev, int func, int flags)
{
	struct pinctrl_ops *ops = pinctrl_get_ops(dev);

	if (!ops->request)
		return -ENOSYS;

	return ops->request(dev, func, flags);
}

int pinctrl_request_noflags(struct udevice *dev, int func)
{
	return pinctrl_request(dev, func, 0);
}

int pinctrl_get_periph_id(struct udevice *dev, struct udevice *periph)
{
	struct pinctrl_ops *ops = pinctrl_get_ops(dev);

	if (!ops->get_periph_id)
		return -ENOSYS;

	return ops->get_periph_id(dev, periph);
}

static int pinctrl_post_bind(struct udevice *dev)
{
	/* Scan the bus for devices */
	return dm_scan_fdt_node(dev, gd->fdt_blob, dev->of_offset, false);
}

UCLASS_DRIVER(pinctrl) = {
	.id		= UCLASS_PINCTRL,
	.name		= "pinctrl",
	.post_bind	= pinctrl_post_bind,
};
