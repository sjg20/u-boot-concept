// SPDX-License-Identifier: GPL-2.0+
/*
 * Simple control of which display/output to use while booting
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#include <alist.h>
#include <bootctl.h>
#include <bootstd.h>
#include <dm.h>
#include <expo.h>
#include "display.h"
#include "oslist.h"
#include "util.h"

/**
 * struct display_priv - information about the display
 *
 * @osinfo: List of OSes to show
 */
struct display_priv {
	struct alist osinfo;
	struct expo *expo;
	bool need_refresh;
};

/*
static int refresh(struct udevice *dev)
{
	struct display_priv *priv = dev_get_priv(dev);
	struct osinfo *info;
	int option = 0;

	alist_for_each(info, &priv->osinfo)
		bc_printf(dev, "%c  %s\n", option++ + '1', info->bflow.os_name);

	return 0;
}
*/

static int simple_display_probe(struct udevice *dev)
{
	struct display_priv *priv = dev_get_priv(dev);

	alist_init_struct(&priv->osinfo, struct osinfo);

	return 0;
}

static int simple_display_bind(struct udevice *dev)
{
	struct bootctl_uc_plat *ucp = dev_get_uclass_plat(dev);

	ucp->desc = "Graphical or textual display for user";

	return 0;
}

static int simple_display_print(struct udevice *dev, const char *msg)
{
	printf("%s", msg);

	return 0;
}

static int simple_display_show(struct udevice *dev)
{
	struct display_priv *priv = dev_get_priv(dev);
	struct bootstd_priv *std;

	LOGR("sdb", bootstd_get_priv(&std));
	LOGR("sds", bootflow_menu_start(std, true, &priv->expo));

	return 0;
}

static int simple_display_add(struct udevice *dev, struct osinfo *info)
{
	struct display_priv *priv = dev_get_priv(dev);
	int seq = priv->osinfo.count;
	struct scene *scn;

	info = alist_add(&priv->osinfo, *info);
	if (!info)
		return -ENOMEM;
	LOGR("sda", bootflow_menu_add(priv->expo, &info->bflow, seq,
				      &scn));
	priv->need_refresh = true;
	// refresh(dev);

	return 0;
}

static int simple_display_render(struct udevice *dev)
{
	struct display_priv *priv = dev_get_priv(dev);

	if (priv->need_refresh) {
		LOGR("sds", expo_render(priv->expo));
		priv->need_refresh = false;
	}

	return 0;
}

static int simple_display_poll(struct udevice *dev, struct osinfo **infop)
{
	struct display_priv *priv = dev_get_priv(dev);
	struct bootstd_priv *std;
	struct osinfo *info;
	int seq, ret;

	ret = bootflow_menu_poll(priv->expo, &seq);
	if (ret == -ERESTART)
		priv->need_refresh = true;
	LOGR("sdp", ret);

	LOGR("sdb", bootstd_get_priv(&std));
	info = alist_getw(&priv->osinfo, seq, struct osinfo);
	*infop = info;

	return 0;
}

static struct bc_display_ops ops = {
	.print	= simple_display_print,
	.show	= simple_display_show,
	.add	= simple_display_add,
	.render	= simple_display_render,
	.poll	= simple_display_poll,
};

static const struct udevice_id simple_display_ids[] = {
	{ .compatible = "bootctl,simple-display" },
	{ .compatible = "bootctl,display" },
	{ }
};

U_BOOT_DRIVER(simple_display) = {
	.name		= "simple_display",
	.id		= UCLASS_BOOTCTL,
	.of_match	= simple_display_ids,
	.bind		= simple_display_bind,
	.probe		= simple_display_probe,
	.ops		= &ops,
	.priv_auto	= sizeof(struct display_priv),
};
