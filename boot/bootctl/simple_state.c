// SPDX-License-Identifier: GPL-2.0+
/*
 * Provides a simple name/value pair
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#include <alist.h>
#include <abuf.h>
#include <bootctl.h>
#include <bootmeth.h>
#include <bootstd.h>
#include <dm.h>
#include <fs.h>
#include <log.h>
#include <malloc.h>
#include <linux/sizes.h>
#include "state.h"

enum {
	MAX_LINE_LEN	= 250,
};

struct keyval {
	char *key;
	char *val;
};

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
	struct alist items;
};

static int simple_state_load(struct udevice *dev)
{
	struct simple_state_priv *priv = dev_get_priv(dev);
	struct membuf inf;
	struct abuf buf;
	char line[MAX_LINE_LEN];
	bool ok;
	int len;

	LOGR("ssa", fs_load_alloc(priv->ifname, priv->dev_part, priv->filename,
				  SZ_64K, 0, &buf));
	membuf_init(&inf, buf.data, buf.size);
	for (ok = true;
	     len = membuf_readline(&inf, line, sizeof(line), ' ', true),
	     len && ok;) {
		struct keyval kv = {strtok(line, "="), strtok(NULL, "=")};

		if (kv.key && kv.val && !alist_add(&priv->items, kv))
			ok = false;
	}

	abuf_uninit(&buf);

	if (!ok) {
		struct keyval *kv;

		/* avoid memory leaks */
		alist_for_each(kv, &priv->items) {
			free(kv->key);
			free(kv->val);
		}

		return log_msg_ret("ssr", -ENOMEM);
	}

	return 0;
}

static int simple_state_write(struct udevice *dev)
{
	struct simple_state_priv *priv = dev_get_priv(dev);
	struct membuf inf;
	struct keyval *kv;
	struct abuf buf;
	loff_t actwrite;
	int ret;

	abuf_init(&buf);
	if (!abuf_realloc(&buf, SZ_4K))
		LOGR("ssa", -ENOMEM);

	membuf_init(&inf, buf.data, buf.size);

	alist_for_each(kv, &priv->items) {
		char str[MAX_LINE_LEN];
		int len;

		len = snprintf(str, MAX_LINE_LEN, "%s = %s\n", kv->key,
			       kv->val);
		if (membuf_put(&inf, str, len + 1) != len + 1)
			ret = log_msg_ret("ssp", -ENOSPC);
	}

	if (!ret) {
		ret = fs_set_blk_dev(priv->ifname, priv->dev_part, FS_TYPE_ANY);
		if (ret)
			ret = log_msg_ret("sss", ret);
		else {
			ret = fs_write(priv->filename, abuf_addr(&buf), 0,
				       buf.size, &actwrite);
			if (ret)
				ret = log_msg_ret("ssw", ret);
		}
	}

	abuf_uninit(&buf);
	if (ret)
		return ret;

	return 0;
}

static int simple_state_probe(struct udevice *dev)
{
	struct simple_state_priv *priv = dev_get_priv(dev);

	alist_init_struct(&priv->items, struct keyval);

	return 0;
}

static int simple_state_of_to_plat(struct udevice *dev)
{
	struct simple_state_priv *priv = dev_get_priv(dev);

	LOGR("ssi", dev_read_string_index(dev, "location", 0, &priv->ifname));
	LOGR("ssd", dev_read_string_index(dev, "location", 1, &priv->dev_part));
	priv->filename = dev_read_string(dev, "filename");
	if (!priv->ifname || !priv->dev_part || !priv->filename)
		LOGR("ssp", -EINVAL);

	return 0;
}

static int simple_state_bind(struct udevice *dev)
{
	struct bootctl_uc_plat *ucp = dev_get_uclass_plat(dev);

	ucp->desc = "Stores state information about booting";

	return 0;
}

static struct bc_state_ops ops = {
	.load	= simple_state_load,
	.save	= simple_state_write,
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
	.ops		= &ops,
	.bind		= simple_state_bind,
	.probe		= simple_state_probe,
	.of_to_plat	= simple_state_of_to_plat,
	.priv_auto	= sizeof(struct simple_state_priv),
};
