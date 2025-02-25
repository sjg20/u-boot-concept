// SPDX-License-Identifier: GPL-2.0+
/*
 * Simple control of which display/output to use while booting
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#include <abuf.h>
#include <bootctl.h>
#include <bootmeth.h>
#include <bootstd.h>
#include <dm.h>
#include <fs.h>
#include <log.h>
#include "state.h"

/**
 * struct simple_state_priv - Private data for simple-state
 *
 * @ifname: Interface which stores the state
 * @dev_part: Device and partition number which stores the state
 * @filename: Filename which stores the state
 */
struct simple_state_priv {
	const char *ifname;
	const char *dev_part;
	const char *filename;
};

static int simple_state_bind(struct udevice *dev)
{
	struct bootctl_uc_plat *ucp = dev_get_uclass_plat(dev);

	ucp->desc = "Stores state information about booting";

	return 0;
}

static int simple_state_read(struct udevice *dev)
{
	struct simple_state_priv *priv = dev_get_priv(dev);
	struct abuf buf;

	LOGR("ssa", fs_load_alloc(priv->ifname, priv->dev_part, priv->filename,
				  SZ_64K, 0, &buf));

	abuf_uninit(&buf);

	return 0;
}

static int simple_state_of_to_plat(struct udevice *dev)
{
	struct simple_state_priv *priv = dev_get_priv(dev);

	LOGR("ssi", dev_read_string_index(dev, "ifname", 0, &priv->ifname));
	LOGR("ssd", dev_read_string_index(dev, "dev-part", 0, &priv->dev_part));
	priv->filename = dev_read_string(dev, "filename");
	if (!priv->ifname || !priv->dev_part || !priv->filename)
		LOGR("ssp", -EINVAL);

	return 0;
}

static struct bc_state_ops ops = {
	.read	= simple_state_read,
};

static const struct udevice_id simple_state_ids[] = {
	{ .compatible = "bootctl,simple-state" },
	{ .compatible = "bootctl,state" },
	{ }
};

U_BOOT_DRIVER(simple_state) = {
	.name		= "simple_state",
	.id		= UCLASS_BOOTCTL_STATE,
	.of_match	= simple_state_ids,
	.bind		= simple_state_bind,
	.of_to_plat	= simple_state_of_to_plat,
	.ops		= &ops,
};
