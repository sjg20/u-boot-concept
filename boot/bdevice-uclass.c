// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootdevice.h>
#include <bootflow.h>
#include <dm.h>

int bootdevice_get_bootflow(struct udevice *dev, struct bootflow_iter *iter,
			    struct bootflow *bflow)
{
	const struct bootdevice_ops *ops = bootdevice_get_ops(dev);

	if (!ops->get_bootflow)
		return -ENOSYS;
	memset(bflow, '\0', sizeof(*bflow));
	bflow->dev = dev;
	bflow->method = iter->method;

	return ops->get_bootflow(dev, iter, bflow);
}

static int bootdevice_init(struct uclass *uc)
{
	struct bootdevice_state *state = uclass_get_priv(uc);

	INIT_LIST_HEAD(&state->glob_head);

	return 0;
}

static int bootdevice_post_bind(struct udevice *dev)
{
	struct bootdevice_uc_plat *ucp = dev_get_uclass_plat(dev);

	INIT_LIST_HEAD(&ucp->bootflow_head);

	return 0;
}

UCLASS_DRIVER(bootdevice) = {
	.id		= UCLASS_BOOTDEVICE,
	.name		= "bootdevice",
	.flags		= DM_UC_FLAG_SEQ_ALIAS,
	.priv_auto	= sizeof(struct bootdevice_state),
	.per_device_plat_auto	= sizeof(struct bootdevice_uc_plat),
	.init		= bootdevice_init,
	.post_bind	= bootdevice_post_bind,
};
