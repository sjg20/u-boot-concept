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
#include <video_console.h>
#include "logic.h"
#include "oslist.h"
#include "ui.h"
#include "util.h"
#include "../bootflow_internal.h"

/* Define to 1 to use text mode (for terminals), 0 for graphics */
#define TEXT_MODE	0

/**
 * struct ui_priv - information about the display
 *
 * @expo: Expo containing the menu
 * @scn: Current scene being shown
 * @priv: Logic status
 * @need_refresh: true if the display needs a refresh
 * @console: vidconsole device in use
 * @autoboot_template: template string to use for autoboot
 * @autoboot_str: Current string displayed for autoboot timeout
 */
struct ui_priv {
	struct expo *expo;
	struct scene *scn;		/* consider dropping this */
	struct logic_priv *lpriv;
	bool need_refresh;
	struct udevice *console;
	const char *autoboot_template;
	char autoboot_str[200];
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
	struct udevice *ldev;

	LOGR("sup", uclass_first_device_err(UCLASS_BOOTCTL, &ldev));

	priv->lpriv = dev_get_priv(ldev);

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
	const char *old_str;
	struct scene *scn;
	uint scene_id;
	int ret;

	LOGR("sdb", bootstd_get_priv(&std));
	LOGR("sds", bootflow_menu_setup(std, TEXT_MODE, &priv->expo));

	old_str = expo_set_str(priv->expo, STR_AUTOBOOT, priv->autoboot_str);
	priv->autoboot_template = old_str;
	strlcpy(priv->autoboot_str, old_str, sizeof(priv->autoboot_str));

	printf("theme %s\n", ofnode_get_name(std->theme));

	if (ofnode_valid(std->theme))
		LOGR("thm", expo_apply_theme(priv->expo, std->theme));

	ret = expo_first_scene_id(priv->expo);
	if (ret < 0)
		return log_msg_ret("ufs", ret);
	scene_id = ret;
	scn = expo_lookup_scene_id(priv->expo, scene_id);

	LOGR("ecd", expo_calc_dims(priv->expo));

	LOGR("usa", scene_arrange(scn));

	scene_set_highlight_id(scn, OBJ_MENU);
	priv->scn = scn;

	LOGR("suq", device_find_first_child_by_uclass(priv->expo->display,
						      UCLASS_VIDEO_CONSOLE,
						      &priv->console));
	vidconsole_set_quiet(priv->console, true);

	return 0;
}

static int simple_ui_add(struct udevice *dev, struct osinfo *info)
{
	struct ui_priv *priv = dev_get_priv(dev);
	struct logic_priv *lpriv = priv->lpriv;
	int seq = lpriv->osinfo.count;
	struct bootstd_priv *std;
	struct scene *scn;

	info = alist_add(&lpriv->osinfo, *info);
	if (!info)
		return -ENOMEM;
	LOGR("sda", bootflow_menu_add(priv->expo, &info->bflow, seq,
				      &scn));

	LOGR("sup", bootstd_get_priv(&std));
	if (ofnode_valid(std->theme)) {
		LOGR("thm", expo_apply_theme(priv->expo, std->theme));
		// LOGR("th2", bootflow_menu_apply_theme(priv->expo, std->theme));
	}
	LOGR("ecd", expo_calc_dims(priv->expo));

	if (lpriv->default_os &&
	    !strcmp(lpriv->default_os, info->bflow.os_name))
		scene_menu_set_point_item(scn, OBJ_MENU, ITEM + seq);

	priv->need_refresh = true;
	// refresh(dev);

	return 0;
}

static int simple_ui_render(struct udevice *dev)
{
	struct ui_priv *priv = dev_get_priv(dev);

	snprintf(priv->autoboot_str, sizeof(priv->autoboot_str),
		 priv->autoboot_template, priv->lpriv->autoboot_remain_s);

	if (priv->need_refresh || !TEXT_MODE) {
		LOGR("sds", expo_render(priv->expo));
		priv->need_refresh = false;
	}

	return 0;
}

static int simple_ui_poll(struct udevice *dev, int *seqp, bool *selectedp)
{
	struct ui_priv *priv = dev_get_priv(dev);
	struct logic_priv *lpriv = priv->lpriv;
	int seq, ret;
	bool ok = true;

	*seqp = -1;
	ret = bootflow_menu_poll(priv->expo, &seq);
	ok = !ret;
	if (ret == -ERESTART) {
		priv->need_refresh = true;
		lpriv->autoboot_active = false;
		scene_obj_set_hide(priv->scn, OBJ_AUTOBOOT, true);
		ok = true;
	} else if (ret == -EAGAIN) {
		ok = true;
	}

	*seqp = seq;
	if (ret) {
		if (!ok)
			return log_msg_ret("sdp", ret);
	} else {
		*selectedp = true;
	}

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
