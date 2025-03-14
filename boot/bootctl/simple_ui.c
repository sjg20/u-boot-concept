// SPDX-License-Identifier: GPL-2.0+
/*
 * Provides a simple boot menu on a graphical display
 *
 * TODO: Support a text display / serial terminal
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#define LOG_CATEGORY	UCLASS_BOOTCTL

#include <alist.h>
#include <bootctl.h>
#include <bootstd.h>
#include <dm.h>
#include <expo.h>
#include <video_console.h>
#include <bootctl/logic.h>
#include <bootctl/oslist.h>
#include <bootctl/ui.h>
#include <bootctl/util.h>
#include "../bootflow_internal.h"

/* TODO: Define to 1 to use text mode (for terminals), 0 for graphics */
#define TEXT_MODE	0

/**
 * struct ui_priv - information about the display
 *
 * @expo: Expo containing the menu
 * @scn: Current scene being shown
 * @lpriv: Private data of logic device
 * @console: vidconsole device in use
 * @autoboot_template: template string to use for autoboot
 * @autoboot_str: current string displayed for autoboot timeout
 * @logo: logo in bitmap format, NULL to use default
 * @logo_size: size of the logo in bytes
 */
struct ui_priv {
	struct expo *expo;
	struct scene *scn;		/* consider dropping this */
	struct logic_priv *lpriv;
	struct udevice *console;
	struct abuf autoboot_template;
	struct abuf *autoboot_str;
	const void *logo;
	int logo_size;
};

static int simple_ui_probe(struct udevice *dev)
{
	struct ui_priv *priv = dev_get_priv(dev);
	struct udevice *ldev;
	int ret;

	ret = uclass_first_device_err(UCLASS_BOOTCTL, &ldev);
	if (ret)
		return log_msg_ret("sup", ret);

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
	struct scene *scn;
	struct abuf *buf;
	uint scene_id;
	int ret;

	ret = bootstd_get_priv(&std);
	if (ret)
		return log_msg_ret("sdb", ret);
	ret = bootflow_menu_setup(std, TEXT_MODE, &priv->expo);
	if (ret)
		return log_msg_ret("sds", ret);

	ret = expo_first_scene_id(priv->expo);
	if (ret < 0)
		return log_msg_ret("ufs", ret);
	scene_id = ret;
	scn = expo_lookup_scene_id(priv->expo, scene_id);

	scene_obj_set_hide(scn, OBJ_AUTOBOOT, false);
	ret = expo_edit_str(priv->expo, STR_AUTOBOOT,
				  &priv->autoboot_template,
				  &priv->autoboot_str);
	if (ret)
		return log_msg_ret("ses", ret);
	ret = expo_edit_str(priv->expo, STR_MENU_TITLE, NULL, &buf);
	if (ret)
		return log_msg_ret("set", ret);
	abuf_printf(buf, "Sourceboot (boot schema)");

	if (priv->logo) {
		ret = scene_img_set_data(scn, OBJ_U_BOOT_LOGO,
					       priv->logo, priv->logo_size);
		if (ret)
			return log_msg_ret("log", ret);
		ret = scene_obj_set_pos(scn, OBJ_U_BOOT_LOGO, 1135, 10);
		if (ret)
			return log_msg_ret("lop", ret);
	}

	log_debug("theme '%s'\n", ofnode_get_name(std->theme));

	if (ofnode_valid(std->theme)) {
		ret = expo_setup_theme(priv->expo, std->theme);
		if (ret)
			return log_msg_ret("thm", ret);
	}

	ret = scene_arrange(scn);
	if (ret)
		return log_msg_ret("usa", ret);

	scene_set_highlight_id(scn, OBJ_MENU);
	priv->scn = scn;

	ret = device_find_first_child_by_uclass(priv->expo->display,
						      UCLASS_VIDEO_CONSOLE,
						      &priv->console);
	if (ret)
		return log_msg_ret("suq", ret);
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
	int ret;

	info = alist_add(&lpriv->osinfo, *info);
	if (!info)
		return -ENOMEM;
	ret = bootflow_menu_add(priv->expo, &info->bflow, seq, &scn);
	if (ret)
		return log_msg_ret("sda", ret);

	ret = bootstd_get_priv(&std);
	if (ret)
		return log_msg_ret("sup", ret);
	if (ofnode_valid(std->theme)) {
		ret = expo_setup_theme(priv->expo, std->theme);
		if (ret)
			return log_msg_ret("thm", ret);
	}
	ret = expo_calc_dims(priv->expo);
	if (ret)
		return log_msg_ret("ecd", ret);

	if (lpriv->default_os &&
	    !strcmp(lpriv->default_os, info->bflow.os_name))
		scene_menu_select_item(scn, OBJ_MENU, ITEM + seq);
	ret = scene_arrange(scn);
	if (ret)
		return log_msg_ret("sua", ret);

	return 0;
}

static int simple_ui_render(struct udevice *dev)
{
	struct ui_priv *priv = dev_get_priv(dev);
	int ret;

	ret = abuf_printf(priv->autoboot_str,
				priv->autoboot_template.data,
				priv->lpriv->autoboot_remain_s);
	if (ret < 0)
		return log_msg_ret("uip", ret);

	ret = expo_arrange(priv->expo);
	if (ret)
		return log_msg_ret("sda", ret);
	ret = expo_render(priv->expo);
	if (ret)
		return log_msg_ret("sds", ret);

	return 0;
}

static int simple_ui_poll(struct udevice *dev, int *seqp, bool *selectedp)
{
	struct ui_priv *priv = dev_get_priv(dev);
	struct logic_priv *lpriv = priv->lpriv;
	int seq, ret;
	bool ok = true;

	*seqp = -1;
	*selectedp = false;
	ret = bootflow_menu_poll(priv->expo, &seq);
	ok = !ret;
	if (ret == -ERESTART || ret == -EREMCHG) {
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
		if (ret == -EAGAIN || ret == -ERESTART)
			return 0;
		return 1;
	}

	*selectedp = true;

	return 0;
}

static int simple_ui_of_to_plat(struct udevice *dev)
{
	struct ui_priv *priv = dev_get_priv(dev);

	priv->logo = dev_read_prop(dev, "logo", &priv->logo_size);

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
	.of_to_plat	= simple_ui_of_to_plat,
	.bind		= simple_ui_bind,
	.probe		= simple_ui_probe,
	.ops		= &ops,
	.priv_auto	= sizeof(struct ui_priv),
};
