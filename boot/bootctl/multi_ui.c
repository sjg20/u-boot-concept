// SPDX-License-Identifier: GPL-2.0+
/*
 * Provides a 'multiboot' menu on a graphical display
 *
 * This is based on Heinrich's design shared in late August.
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
#include <video.h>
#include <video_console.h>
#include <bootctl/logic.h>
#include <bootctl/oslist.h>
#include <bootctl/ui.h>
#include <bootctl/util.h>
#include "../bootflow_internal.h"
#include "../scene_internal.h"

enum {
	/* Bar on the left */
	BAR_X		= 0,
	BAR_Y		= 0,
	BAR_W		= 70,
	BAR_H		= 800,

	HELP_Y		= 675,
	SETTINGS_Y	= 720,

	MAIN_X		= 150,
	MAIN_Y		= 150,

	IMAGES_Y	= 225,
	BOX_W		= 300,
	BOX_H		= 300,
	BOX_MARGIN	= 10,

	// gap between boxes
	GAP_X		= 20,
	GAP_Y		= 20,
};

/**
 * struct multiboot_ui_priv - Driver-specific private data for multiboot UI
 *
 * @use_bootflow_props: true to use bootflow_menu_set_props(), false for multiboot_ui_set_props()
 */
struct multiboot_ui_priv {
	bool use_bootflow_props;
};

static int multiboot_ui_probe(struct udevice *dev)
{
	struct bc_ui_priv *upriv = dev_get_uclass_priv(dev);
	struct udevice *ldev;
	int ret;

	ret = uclass_first_device_err(UCLASS_BOOTCTL, &ldev);
	if (ret)
		return log_msg_ret("sup", ret);

	upriv->lpriv = dev_get_priv(ldev);

	return 0;
}

static int multiboot_ui_bind(struct udevice *dev)
{
	struct bootctl_uc_plat *ucp = dev_get_uclass_plat(dev);

	ucp->desc = "Graphical or textual display for user";

	return 0;
}

int setup_version(struct expo *exp, int i, const struct bootflow *bflow,
		  const char **versp)
{
	const char *vers = NULL;
	struct abuf *buf;
	char *str;
	int ret;

	ret = expo_edit_str(exp, STR_DESC + i, NULL, &buf);
	if (ret)
		return ret;
	abuf_printf(buf, bflow->os_name ? bflow->os_name : bflow->name);

	str = buf->data;
	if (!strncmp("Ubuntu", str, 6) && strlen(str) > 20) {
		/* get the space string after the 24.04[.1] */
		char *p = strchr(str + 8, ' ');

		/* get the space after that */
		char *q = p ? strchr(p + 1, ' ') : NULL;

		vers = str + 7;
		if (q)
			*q = '\0';
		else
			*p = '\0';
	}

	if (versp)
		*versp = vers;

	return 0;
}

static int multiboot_set_item_props(struct scene *scn, int i,
				    const struct bootflow *bflow)
{
	struct expo *exp = scn->expo;
	struct abuf *buf;
	int x, y, ret = 0;

	x = MAIN_X + i * (BOX_W + GAP_X);
	y = IMAGES_Y;

	ret = setup_version(exp, i, bflow, NULL);

	scene_obj_set_bbox(scn, ITEM_BOX + i, x, IMAGES_Y,
			   x + BOX_W, IMAGES_Y + BOX_H);
	scene_obj_set_hide(scn, ITEM_BOX + i, false);

	scene_obj_set_pos(scn, ITEM_DESC + i, x + BOX_MARGIN,
			  IMAGES_Y + 80);
	scene_obj_set_pos(scn, ITEM_LABEL + i, x + BOX_MARGIN,
			  IMAGES_Y + 80 + 20);
	scene_obj_set_pos(scn, ITEM_VERSION_NAME + i, x + BOX_MARGIN,
			  IMAGES_Y + 80 + 70);
	scene_obj_set_pos(scn, ITEM_PREVIEW + i, x + BOX_MARGIN,
			  IMAGES_Y + 5);
	scene_obj_set_pos(scn, ITEM_VERIFIED + i,
			  x + BOX_MARGIN + 40 + 32, IMAGES_Y + 80 + 21);

	ret |= scene_obj_set_hide(scn, ITEM_PREVIEW + i, false);
	ret |= scene_obj_set_hide(scn, ITEM_BOX + i, false);
	ret |= scene_obj_set_hide(scn, ITEM_VERSION_NAME + i, false);
	ret |= scene_obj_set_hide(scn, ITEM_VERIFIED + i, false);

	/* Hide key in multiboot mode (not used with mouse) */
	ret |= scene_obj_set_hide(scn, ITEM_KEY + i, true);
	if (ret)
		return log_msg_ret("msp", ret);

	/* Set font sizes for multiboot UI */
	ret = scene_txt_set_font(scn, ITEM_LABEL + i, "ubuntu_light", 18);
	ret |= scene_txt_set_font(scn, ITEM_DESC + i, "ubuntu_bold", 20);
	ret |= scene_txt_set_font(scn, ITEM_VERSION_NAME + i, NULL, 18);
	if (ret)
		return log_msg_ret("msq", ret);

	ret = expo_edit_str(exp, STR_LABEL + i, NULL, &buf);
	if (!ret)
		abuf_printf(buf, "Canonical");

	return 0;
}

static int multiboot_ui_set_props(struct udevice *dev, struct scene *scn,
				  struct bootstd_priv *std)
{
	struct bc_ui_priv *upriv = dev_get_uclass_priv(dev);
	struct logic_priv *lpriv = upriv->lpriv;
	struct expo *exp = scn->expo;
	struct expo_theme *theme = &exp->theme;
	struct abuf *buf;
	int i, ret = 0;

	/* Set multiboot-specific strings */
	ret = expo_edit_str(exp, STR_PROMPT1B, NULL, &buf);
	if (!ret)
		abuf_printf(buf, "Select image to boot");

	ret = expo_edit_str(exp, STR_PROMPT2, NULL, &buf);
	if (!ret)
		abuf_printf(buf, "Images");

	/* Show multiboot-specific objects */
	scene_obj_set_hide(scn, OBJ_BOX, false);
	scene_obj_set_hide(scn, OBJ_OTHER_LOGO, false);
	scene_obj_set_hide(scn, OBJ_SETTINGS, false);
	scene_obj_set_hide(scn, OBJ_HELP, false);
	scene_obj_set_hide(scn, OBJ_POINTER, true);
	scene_menu_set_pointer(scn, OBJ_MENU, 0);

	/* Enable mouse for multiboot UI */
	expo_set_mouse_enable(exp, true);

	/* Use manual positioning for menu */
	scene_obj_set_manual(scn, OBJ_MENU, true);

	ret = expo_edit_str(upriv->expo, STR_MENU_TITLE, NULL, &buf);
	if (ret)
		return log_msg_ret("set", ret);
	abuf_printf(buf, "Welcome to Multiboot");

	scene_obj_set_halign(scn, OBJ_MENU_TITLE, SCENEOA_LEFT);
	scene_obj_set_pos(scn, OBJ_MENU_TITLE, MAIN_X, 50);

	scene_obj_set_pos(scn, OBJ_PROMPT1B, MAIN_X, 120);
	scene_obj_set_halign(scn, OBJ_PROMPT1B, SCENEOA_LEFT);

	scene_obj_set_pos(scn, OBJ_PROMPT2, MAIN_X, 180);
	scene_obj_set_halign(scn, OBJ_PROMPT2, SCENEOA_LEFT);

	scene_obj_set_hide(scn, OBJ_AUTOBOOT, !lpriv->opt_autoboot);

	if (upriv->logo) {
		ret = scene_obj_set_pos(scn, OBJ_U_BOOT_LOGO, 1045, 10);
		if (ret)
			return log_msg_ret("lop", ret);
	}

	scene_obj_set_bbox(scn, OBJ_BOX, BAR_X, BAR_Y, BAR_X + BAR_W,
			   BAR_Y + BAR_H);
	scene_box_set_fill(scn, OBJ_BOX, true);

	scene_obj_set_bbox(scn, OBJ_OTHER_LOGO, BAR_X, BAR_Y, BAR_X + BAR_W,
			   BAR_Y + 50);
	scene_obj_set_halign(scn, OBJ_OTHER_LOGO, SCENEOA_CENTRE);

	scene_obj_set_bbox(scn, OBJ_SETTINGS, BAR_X, SETTINGS_Y, BAR_X + BAR_W,
			   SETTINGS_Y + 24);
	scene_obj_set_halign(scn, OBJ_SETTINGS, SCENEOA_CENTRE);

	scene_obj_set_bbox(scn, OBJ_HELP, BAR_X, HELP_Y, BAR_X + BAR_W,
			   HELP_Y + 24);
	scene_obj_set_halign(scn, OBJ_HELP, SCENEOA_CENTRE);

	if (ofnode_valid(std->theme)) {
		ret = expo_setup_theme(upriv->expo, std->theme);
		if (ret)
			return log_msg_ret("thm", ret);
	}

	theme->white_on_black = false;

	ret = expo_apply_theme(exp, true);
	if (ret)
		return log_msg_ret("asn", ret);

	scene_obj_set_manual(scn, OBJ_MENU, true);

	for (i = 0; i < std->bootflows.count; i++) {
		const struct bootflow *bflow;

		bflow = alist_get(&std->bootflows, i, struct bootflow);
		if (!bflow)
			return log_msg_ret("mbb", -ENOENT);

		multiboot_set_item_props(scn, i, bflow);
	}

	expo_set_mouse_enable(exp, true);

	scene_txt_set_font(scn, OBJ_MENU_TITLE, NULL, 60);
	scene_txt_set_font(scn, OBJ_PROMPT1B, NULL, 30);
	scene_txt_set_font(scn, OBJ_PROMPT2, "ubuntu_bold", 30);

	scene_menu_select_item(scn, OBJ_MENU, 0);
	scene_set_highlight_id(scn, 0);
	exp->show_highlight = false;

	return 0;
}

static int multiboot_ui_print(struct udevice *dev, const char *msg)
{
	printf("%s", msg);

	return 0;
}

static int multiboot_ui_show(struct udevice *dev)
{
	struct bc_ui_priv *upriv = dev_get_uclass_priv(dev);
	struct multiboot_ui_priv *priv = dev_get_priv(dev);
	struct bootstd_priv *std;
	struct scene *scn;
	struct abuf *buf;
	uint scene_id;
	int ret;

	ret = bootstd_get_priv(&std);
	if (ret)
		return log_msg_ret("sdb", ret);
	ret = bootflow_menu_setup(std, 0, &upriv->expo);
	if (ret)
		return log_msg_ret("sds", ret);

	expo_set_mouse_enable(upriv->expo, true);

	ret = expo_first_scene_id(upriv->expo);
	if (ret < 0)
		return log_msg_ret("ufs", ret);
	scene_id = ret;
	scn = expo_lookup_scene_id(upriv->expo, scene_id);

	ret = expo_edit_str(upriv->expo, STR_PROMPT1B, NULL, &buf);
	if (!ret)
		abuf_printf(buf, "Select image to boot");

	ret = expo_edit_str(upriv->expo, STR_PROMPT2, NULL, &buf);
	if (!ret)
		abuf_printf(buf, "Images");

	ret = expo_edit_str(upriv->expo, STR_AUTOBOOT,
			    &upriv->autoboot_template,
			    &upriv->autoboot_str);
	if (ret)
		return log_msg_ret("ses", ret);

	if (upriv->logo) {
		ret = scene_img_set_data(scn, OBJ_U_BOOT_LOGO, upriv->logo,
					 upriv->logo_size);
		if (ret)
			return log_msg_ret("log", ret);
	}

	ret |= scene_img(scn, "multipass", OBJ_OTHER_LOGO,
			 video_image_getptr(multipass), NULL);

	ret |= scene_img(scn, "settings", OBJ_SETTINGS,
			 video_image_getptr(settings), NULL);

	ret |= scene_img(scn, "help", OBJ_HELP,
			 video_image_getptr(help), NULL);

	log_debug("theme '%s'\n", ofnode_get_name(std->theme));

	if (priv->use_bootflow_props) {
		void *logo = upriv->logo ? (void *)upriv->logo :
				video_get_u_boot_logo(NULL);

		ret = bootflow_menu_set_props(upriv->expo, scn, logo,
					      "Boot Control");
		if (ret)
			return log_msg_ret("bfprops", ret);
	} else {
		ret = multiboot_ui_set_props(dev, scn, std);
		if (ret)
			return log_msg_ret("props", ret);
	}

	ret = scene_arrange(scn);
	if (ret)
		return log_msg_ret("usa", ret);

	upriv->scn = scn;

	ret = device_find_first_child_by_uclass(upriv->expo->display,
						UCLASS_VIDEO_CONSOLE,
						&upriv->console);
	if (ret)
		return log_msg_ret("suq", ret);
	vidconsole_set_quiet(upriv->console, true);
	expo_enter_mode(upriv->expo);

	return 0;
}

static int multiboot_ui_add(struct udevice *dev, struct osinfo *info)
{
	struct bc_ui_priv *upriv = dev_get_uclass_priv(dev);
	struct logic_priv *lpriv = upriv->lpriv;
	struct bootstd_priv *std;
	const char *vers = NULL;
	struct scene *scn;
	struct abuf *buf;
	int seq, ret;

	info = alist_add(&lpriv->osinfo, *info);
	if (!info)
		return log_msg_ret("mua", -ENOMEM);
	seq = bootstd_add_bootflow(&info->bflow);
	if (seq + 1 != lpriv->osinfo.count)
		return log_msg_ret("mdb", seq);
	ret = bootflow_menu_add(upriv->expo, &info->bflow, seq, &scn);
	if (ret)
		return log_msg_ret("mda", ret);

	ret = setup_version(upriv->expo, seq, &info->bflow, &vers);

	if (vers) {
		void *logo;

		ret = expo_edit_str(upriv->expo, STR_LABEL + seq, NULL, &buf);
		if (!ret)
			abuf_printf(buf, "Canonical");

		scene_obj_set_hide(scn, ITEM_VERSION_NAME + seq, false);
		scene_txt_set_font(scn, ITEM_DESC + seq, "ubuntu_bold",
				   20);
		ret = expo_edit_str(upriv->expo, STR_VERSION_NAME + seq, NULL,
				    &buf);
		if (!ret) {
			if (!strncmp("24.04", vers, 5))
				abuf_printf(buf, "Noble Numbat");
			else if (!strncmp("25.04", vers, 5))
				abuf_printf(buf, "Plucky Puffin");
			else if (!strncmp("22.04", vers, 5))
				abuf_printf(buf, "Jammy Jellyfish");
		}

		logo = video_image_getptr(canonical);
		ret |= scene_img(scn, "preview", ITEM_PREVIEW + seq, logo,
				 NULL);

		logo = video_image_getptr(tick);
		ret |= scene_img(scn, "verified", ITEM_VERIFIED + seq, logo,
				 NULL);
	}

	ret = bootstd_get_priv(&std);
	if (ret)
		return log_msg_ret("sup", ret);

	multiboot_set_item_props(scn, seq, &info->bflow);

	ret = expo_calc_dims(upriv->expo);
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

static int multiboot_ui_render(struct udevice *dev)
{
	struct bc_ui_priv *upriv = dev_get_uclass_priv(dev);
	int ret;

	ret = abuf_printf(upriv->autoboot_str,
			  upriv->autoboot_template.data,
			  upriv->lpriv->autoboot_remain_s);
	if (ret < 0)
		return log_msg_ret("uip", ret);

	ret = expo_arrange(upriv->expo);
	if (ret)
		return log_msg_ret("sda", ret);
	ret = expo_render(upriv->expo);
	if (ret)
		return log_msg_ret("sdr", ret);

	return 0;
}

static int multiboot_ui_switch_layout(struct udevice *dev)
{
	struct bc_ui_priv *upriv = dev_get_uclass_priv(dev);
	struct multiboot_ui_priv *priv = dev_get_priv(dev);
	struct bootstd_priv *std;
	struct scene *scn;
	int ret;

	/* Toggle the layout mode */
	priv->use_bootflow_props = !priv->use_bootflow_props;

	/* Get the current scene */
	scn = expo_lookup_scene_id(upriv->expo, upriv->expo->scene_id);
	if (!scn)
		return log_msg_ret("scn", -ENOENT);

	ret = bootstd_get_priv(&std);
	if (ret)
		return log_msg_ret("std", ret);

	/* Re-apply properties with the new layout */
	if (priv->use_bootflow_props) {
		/* Apply simple_ui style using bootflow_menu_set_props() */
		void *logo = upriv->logo ? (void *)upriv->logo :
					     video_get_u_boot_logo(NULL);

		ret = bootflow_menu_set_props(upriv->expo, scn, logo,
					      "Boot control");
		if (ret)
			return log_msg_ret("bfprops", ret);
	} else {
		/* Apply multiboot style using multiboot_ui_set_props() */
		ret = multiboot_ui_set_props(dev, scn, std);
		if (ret)
			return log_msg_ret("props", ret);
	}

	/* Calculate dimensions then re-arrange */
	ret = expo_calc_dims(upriv->expo);
	if (ret)
		return log_msg_ret("ecd", ret);

	ret = scene_arrange(scn);
	if (ret)
		return log_msg_ret("arr", ret);

	return 0;
}

static int multiboot_ui_poll(struct udevice *dev, int *seqp, bool *selectedp)
{
	struct bc_ui_priv *upriv = dev_get_uclass_priv(dev);
	struct logic_priv *lpriv = upriv->lpriv;
	int seq, ret;
	bool ok = true;

	*seqp = -1;
	*selectedp = false;
	ret = bootflow_menu_poll(upriv->expo, &seq);

	ok = !ret;
	if (ret == -ERESTART || ret == -EREMCHG) {
		lpriv->autoboot_active = false;
		scene_obj_set_hide(upriv->scn, OBJ_AUTOBOOT, true);
		ok = true;
	} else if (ret == -ECOMM) {
		/* Layout change requested */
		ret = multiboot_ui_switch_layout(dev);
		if (ret)
			return log_msg_ret("swl", ret);
		ret = -ECOMM;  /* Restore the original return code */
		ok = true;
	} else if (ret == -EAGAIN || ret == -ENOTTY) {
		ok = true;
	}

	*seqp = seq;
	if (ret) {
		if (!ok)
			return log_msg_ret("sdp", ret);
		if (ret == -EAGAIN || ret == -ERESTART || ret == -ECOMM ||
		    ret == -ENOTTY)
			return 0;
		return 1;
	}

	*selectedp = true;

	return 0;
}

static int multiboot_ui_of_to_plat(struct udevice *dev)
{
	struct bc_ui_priv *upriv = dev_get_uclass_priv(dev);

	upriv->logo = dev_read_prop(dev, "logo", &upriv->logo_size);

	return 0;
}

static struct bc_ui_ops ops = {
	.print	= multiboot_ui_print,
	.show	= multiboot_ui_show,
	.add	= multiboot_ui_add,
	.render	= multiboot_ui_render,
	.poll	= multiboot_ui_poll,
	.switch_layout = multiboot_ui_switch_layout,
};

static const struct udevice_id multiboot_ui_ids[] = {
	{ .compatible = "bootctl,multiboot-ui" },
	{ .compatible = "bootctl,ui" },
	{ }
};

U_BOOT_DRIVER(multiboot_ui) = {
	.name		= "multiboot_ui",
	.id		= UCLASS_BOOTCTL_UI,
	.of_match	= multiboot_ui_ids,
	.of_to_plat	= multiboot_ui_of_to_plat,
	.bind		= multiboot_ui_bind,
	.probe		= multiboot_ui_probe,
	.ops		= &ops,
	.priv_auto	= sizeof(struct multiboot_ui_priv),
};
