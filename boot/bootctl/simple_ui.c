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
#include "oslist.h"
#include "ui.h"
#include "util.h"

/* Define to 1 to use text mode (for terminals), 0 for graphics */
#define TEXT_MODE	0

/**
 * struct ui_priv - information about the display
 *
 * @osinfo: List of OSes to show
 */
struct ui_priv {
	struct alist osinfo;
	struct expo *expo;
	bool need_refresh;
};

/*
static int refresh(struct udevice *dev)
{
	struct ui_priv *priv = dev_get_priv(dev);
	struct osinfo *info;
	int option = 0;

	alist_for_each(info, &priv->osinfo)
		bc_printf(dev, "%c  %s\n", option++ + '1', info->bflow.os_name);

	return 0;
}
*/

static int simple_ui_probe(struct udevice *dev)
{
	struct ui_priv *priv = dev_get_priv(dev);

	alist_init_struct(&priv->osinfo, struct osinfo);

	return 0;
}

static int simple_ui_bind(struct udevice *dev)
{
	struct bootctl_uc_plat *ucp = dev_get_uclass_plat(dev);

	ucp->desc = "Graphical or textual display for user";

	return 0;
}

static int simple_ui_print(struct udevice *dev, const char *msg)
{
	printf("%s", msg);

	return 0;
}

static int simple_ui_show(struct udevice *dev)
{
	struct ui_priv *priv = dev_get_priv(dev);
	struct bootstd_priv *std;

	LOGR("sdb", bootstd_get_priv(&std));
	LOGR("sds", bootflow_menu_start(std, TEXT_MODE, &priv->expo));

	return 0;
}

static int simple_ui_add(struct udevice *dev, struct osinfo *info)
{
	struct ui_priv *priv = dev_get_priv(dev);
	int seq = priv->osinfo.count;
	struct bootstd_priv *std;
	struct scene *scn;

	info = alist_add(&priv->osinfo, *info);
	if (!info)
		return -ENOMEM;
	LOGR("sda", bootflow_menu_add(priv->expo, &info->bflow, seq,
				      &scn));

	LOGR("arr", scene_arrange(scn));

	LOGR("sup", bootstd_get_priv(&std));
	if (ofnode_valid(std->theme))
		LOGR("thm", bootflow_menu_apply_theme(priv->expo, std->theme));

	priv->need_refresh = true;
	// refresh(dev);

	return 0;
}

static int simple_ui_render(struct udevice *dev)
{
	struct ui_priv *priv = dev_get_priv(dev);

	if (priv->need_refresh || !TEXT_MODE) {
		LOGR("sds", expo_render(priv->expo));
		priv->need_refresh = false;
	}

	return 0;
}

static int simple_ui_poll(struct udevice *dev, struct osinfo **infop)
{
	struct ui_priv *priv = dev_get_priv(dev);
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

static struct bc_ui_ops ops = {
	.print	= simple_ui_print,
	.show	= simple_ui_show,
	.add	= simple_ui_add,
	.render	= simple_ui_render,
	.poll	= simple_ui_poll,
};

static const struct udevice_id simple_ui_ids[] = {
	{ .compatible = "bootctl,simple-ui" },
	{ .compatible = "bootctl,ui" },
	{ }
};

U_BOOT_DRIVER(simple_ui) = {
	.name		= "simple_ui",
	.id		= UCLASS_BOOTCTL_UI,
	.of_match	= simple_ui_ids,
	.bind		= simple_ui_bind,
	.probe		= simple_ui_probe,
	.ops		= &ops,
	.priv_auto	= sizeof(struct ui_priv),
};
