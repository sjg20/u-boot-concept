/*
 * A vboot flag controlled by a keypress
 *
 * Copyright (c) 2018 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <asm/sdl.h>
#include <cros/vboot_flag.h>

struct flag_key_priv {
	int key;
};

static int flag_key_read(struct udevice *dev)
{
	struct flag_key_priv *priv = dev_get_priv(dev);

	return sandbox_sdl_key_pressed(priv->key);
}

static int flag_key_probe(struct udevice *dev)
{
	struct flag_key_priv *priv = dev_get_priv(dev);
	u32 value;

	if (dev_read_u32(dev, "key", &value)) {
		log(UCLASS_CROS_VBOOT_FLAG, LOGL_WARNING,
		    "Missing 'key' property for '%s'\n", dev->name);

		return -EINVAL;
	}
	priv->key = value;

	return 0;
}

static const struct vboot_flag_ops flag_key_ops = {
	.read	= flag_key_read,
};

static const struct udevice_id flag_key_ids[] = {
	{ .compatible = "google,key-flag" },
	{ }
};

U_BOOT_DRIVER(flag_key_drv) = {
	.name		= "flag_key",
	.id		= UCLASS_CROS_VBOOT_FLAG,
	.of_match	= flag_key_ids,
	.probe		= flag_key_probe,
	.ops		= &flag_key_ops,
	.priv_auto_alloc_size	= sizeof(struct flag_key_priv),
};
