// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootmethod.h>
#include <dm.h>

int bootmethod_setup(struct udevice *dev, struct bootflow *bflow)
{
	const struct bootmethod_ops *ops = bootmethod_get_ops(dev);

	if (!ops->setup)
		return -ENOSYS;

	return ops->setup(dev, bflow);
}

int bootmethod_boot(struct udevice *dev, struct bootflow *bflow)
{
	const struct bootmethod_ops *ops = bootmethod_get_ops(dev);

	if (!ops->setup)
		return -ENOSYS;

	return ops->boot(dev, bflow);
}

UCLASS_DRIVER(bootmethod) = {
	.id		= UCLASS_BOOTMETHOD,
	.name		= "bootmethod",
	.flags		= DM_UC_FLAG_SEQ_ALIAS,
};
