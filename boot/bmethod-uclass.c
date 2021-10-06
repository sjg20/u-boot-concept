// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootmethod.h>
#include <dm.h>

int bootmethod_read_bootflow(struct udevice *dev, struct bootflow *bflow)
{
	const struct bootmethod_ops *ops = bootmethod_get_ops(dev);

	if (!ops->read_bootflow)
		return -ENOSYS;

	return ops->read_bootflow(dev, bflow);
}

int bootmethod_boot(struct udevice *dev, struct bootflow *bflow)
{
	const struct bootmethod_ops *ops = bootmethod_get_ops(dev);

	if (!ops->boot)
		return -ENOSYS;

	return ops->boot(dev, bflow);
}

int bootmethod_read_file(struct udevice *dev, struct bootflow *bflow,
			 const char *file_path, ulong addr, ulong *sizep)
{
	const struct bootmethod_ops *ops = bootmethod_get_ops(dev);

	if (!ops->read_file)
		return -ENOSYS;

	return ops->read_file(dev, bflow, file_path, addr, sizep);
}

UCLASS_DRIVER(bootmethod) = {
	.id		= UCLASS_BOOTMETHOD,
	.name		= "bootmethod",
	.flags		= DM_UC_FLAG_SEQ_ALIAS,
};
