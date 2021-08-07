// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootmethod.h>
#include <dm.h>
#include <log.h>
#include <dm/lists.h>

enum {
	/*
	 * Set some sort of limit on the number of bootflows a bootmethod can
	 * return
	 */
	MAX_BOOTFLOWS_PER_BOOTMETHOD	= 100,
};

int bootmethod_get_bootflow(struct udevice *dev, int seq,
			    struct bootflow *bflow)
{
	const struct bootmethod_ops *ops = bootmethod_get_ops(dev);

	if (!ops->get_bootflow)
		return -ENOSYS;

	return ops->get_bootflow(dev, seq, bflow);
}

static int next_bootflow(struct udevice *dev, int seq, struct bootflow *bflow)
{
	int ret;

	ret = bootmethod_get_bootflow(dev, seq, bflow);
	if (ret)
		return ret;

	return 0;
}

int bootmethod_first_bootflow(struct bootmethod_iter *iter, int flags,
			      struct bootflow *bflow)
{
	struct udevice *dev;
	int ret;

	iter->flags = flags;
	iter->seq = 0;
	ret = uclass_first_device_err(UCLASS_BOOTMETHOD, &dev);
	if (ret)
		return ret;
	iter->dev = dev;

	ret = bootmethod_next_bootflow(iter, bflow);
	if (ret)
		return ret;

	return 0;
}

int bootmethod_next_bootflow(struct bootmethod_iter *iter,
			     struct bootflow *bflow)
{
	int ret;

	do {
		ret = next_bootflow(iter->dev, iter->seq, bflow);

		/* If we got a valid bootflow, return it */
		if (!ret)
			return 0;

		/* If we got some other error, try the device again */
		else if (ret != -ESHUTDOWN) {
			log_warning("Bootmethod '%s' seq %d: Error %d\n",
				    iter->dev->name, iter->seq, ret);
			if (iter->seq++ == MAX_BOOTFLOWS_PER_BOOTMETHOD)
				return log_msg_ret("max", -E2BIG);
			continue;
		}

		/* we got to the end of that bootmethod, try the next */
		ret = uclass_next_device_err(&iter->dev);

		/* if there are no more bootmethods, give up */
		if (ret)
			return ret;

		/* start at the beginning of this bootmethod */
		iter->seq = 0;
	} while (1);
}

int bootmethod_bind(struct udevice *parent, const char *drv_name,
		    const char *name, struct udevice **devp)
{
	struct udevice *dev;
	int ret;

	ret = device_bind_driver(parent, drv_name, name, &dev);
	if (ret)
		return ret;
	*devp = dev;

	return 0;
}

UCLASS_DRIVER(bootmethod) = {
	.id		= UCLASS_BOOTMETHOD,
	.name		= "bootmethod",
	.flags		= DM_UC_FLAG_SEQ_ALIAS,
	.per_device_auto	= sizeof(struct bootmethod_priv),
};
