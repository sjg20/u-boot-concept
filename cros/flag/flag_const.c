/*
 * Copyright (c) 2017 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <cros/vboot_flag.h>

DECLARE_GLOBAL_DATA_PTR;

struct flag_const_priv {
	bool value;
};

static int flag_const_read(struct udevice *dev)
{
	struct flag_const_priv *priv = dev_get_priv(dev);

	return priv->value;
}

static int flag_const_ofdata_to_platdata(struct udevice *dev)
{
	struct flag_const_priv *priv = dev_get_priv(dev);
	int ret;

	ret = fdtdec_get_int(gd->fdt_blob, dev_of_offset(dev), "value", -1);
	if (ret == -1) {
		debug("%s: Missing flag value in '%s'", __func__, dev->name);
		return ret;
	}
	priv->value = ret != 0;

	return 0;
}

static const struct vboot_flag_ops flag_const_ops = {
	.read	= flag_const_read,
};

static const struct udevice_id flag_const_ids[] = {
	{ .compatible = "google,const-flag" },
	{ }
};

U_BOOT_DRIVER(flag_const_drv) = {
	.name		= "flag_const",
	.id		= UCLASS_CROS_VBOOT_FLAG,
	.of_match	= flag_const_ids,
	.ofdata_to_platdata	= flag_const_ofdata_to_platdata,
	.ops		= &flag_const_ops,
	.priv_auto_alloc_size	= sizeof(struct flag_const_priv),
};
