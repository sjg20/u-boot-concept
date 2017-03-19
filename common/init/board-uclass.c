/*
 * Board driver interface
 *
 * Copyright (c) 2017 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <board.h>

DECLARE_GLOBAL_DATA_PTR;

int board_phase(struct udevice *dev, enum board_phase_t phase)
{
	struct board_uc_priv *priv = dev_get_uclass_priv(dev);
	struct board_ops *ops = board_get_ops(dev);

	if (!ops->phase)
		return -ENOSYS;
	if (!priv->phase_mask && phase == BOARD_PHASE_FIRST) {
		printf("Device '%s' supports no phases\n", dev->name);
		return -EINVAL;
	}
	if (!(priv->phase_mask & board_phase_mask(phase)))
		return -ENOSYS;

	return ops->phase(dev, phase);
}

int board_walk_phase_count(enum board_phase_t phase, bool verbose)
{
	struct udevice *dev;
	int count = 0;
	int ret;

	for (ret = uclass_first_device(UCLASS_BOARD, &dev);
	     dev;
	     uclass_next_device(&dev)) {
		ret = board_phase(dev, phase);
		if (ret == 0) {
			count++;
		} else if (ret == BOARD_PHASE_CLAIMED) {
			count++;
			debug("Phase %d claimed by '%s'\n", phase, dev->name);
			break;
		} else if (ret != -ENOSYS) {
			gd->phase_count[phase] += count;
			return ret;
		}
	}

	if (!count) {
		if (verbose)
			printf("Unable to find driver for phase %d\n", phase);
		return -ENOSYS;
	}
	gd->phase_count[phase] += count;

	return count;
}

int board_walk_phase(enum board_phase_t phase)
{
	int ret;

	ret = board_walk_phase_count(phase, true);
	if (ret < 0)
		return ret;

	return 0;
}

int board_walk_opt_phase(enum board_phase_t phase)
{
	int ret;

	ret = board_walk_phase_count(phase, false);
	if (ret < 0 && ret != -ENOSYS)
		return ret;

	return 0;
}

int board_support_phase(struct udevice *dev, enum board_phase_t phase)
{
	struct board_uc_priv *priv = dev_get_uclass_priv(dev);

	priv->phase_mask |= board_phase_mask(phase);

	return 0;
}

int board_support_phase_mask(struct udevice *dev, ulong phase_mask)
{
	struct board_uc_priv *priv = dev_get_uclass_priv(dev);

	priv->phase_mask = phase_mask;

	return 0;
}

UCLASS_DRIVER(board) = {
	.id		= UCLASS_BOARD,
	.name		= "board",
	.per_device_auto_alloc_size	= sizeof(struct board_uc_priv),
};
