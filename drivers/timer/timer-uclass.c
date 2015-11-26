/*
 * Copyright (C) 2015 Thomas Chou <thomas@wytron.com.tw>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <dm/lists.h>
#include <dm/device-internal.h>
#include <errno.h>
#include <timer.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * Implement a Timer uclass to work with lib/time.c. The timer is usually
 * a 32 bits free-running up counter. The get_rate() method is used to get
 * the input clock frequency of the timer. The get_count() method is used
 * get the current 32 bits count value. If the hardware is counting down,
 * the value should be inversed inside the method. There may be no real
 * tick, and no timer interrupt.
 */

int timer_get_count(struct udevice *dev, unsigned long *count)
{
	const struct timer_ops *ops = device_get_ops(dev);

	if (!ops->get_count)
		return -ENOSYS;

	return ops->get_count(dev, count);
}

unsigned long timer_get_rate(struct udevice *dev)
{
	struct timer_dev_priv *uc_priv = dev_get_uclass_priv(dev);

	return uc_priv->clock_rate;
}

int timer_init(void)
{
	const void *blob = gd->fdt_blob;
	struct udevice *dev;
	int node;

	if (CONFIG_IS_ENABLED(OF_CONTROL) && blob) {
		/* Check for a chosen timer to be used for tick */
		node = fdtdec_get_chosen_node(blob, "tick-timer");
		if (node < 0)
			return -ENODEV;

		if (uclass_get_device_by_of_offset(UCLASS_TIMER, node, &dev)) {
			/*
			 * If the timer is not marked to be bound before
			 * relocation, bind it anyway.
			 */
			if (node > 0 &&
			    !lists_bind_fdt(gd->dm_root, blob, node, &dev)) {
				int ret = device_probe(dev);
				if (ret)
					return ret;
			}
		}
	}

	gd->timer = dev;
	return 0;
}

UCLASS_DRIVER(timer) = {
	.id		= UCLASS_TIMER,
	.name		= "timer",
	.flags		= DM_UC_FLAG_SEQ_ALIAS,
	.per_device_auto_alloc_size = sizeof(struct timer_dev_priv),
};
