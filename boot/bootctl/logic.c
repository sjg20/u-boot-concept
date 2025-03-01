// SPDX-License-Identifier: GPL-2.0+
/*
 * Implementation of the logic to perform a boot
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#include <bootctl.h>
#include <dm.h>
#include <log.h>
#include <time.h>
#include <version.h>
#include <dm/device-internal.h>
#include "logic.h"
#include "oslist.h"
#include "state.h"
#include "ui.h"
#include "util.h"

enum {
	COUNTDOWN_INTERVAL_MS	= 1000,	/* inteval between countdown updates */
};

static int logic_start(struct udevice *dev)
{
	struct logic_priv *priv = dev_get_priv(dev);

	/* figure out the UI to use */
	LOGR("bgd", bootctl_get_dev(UCLASS_BOOTCTL_UI, &priv->ui));

	/* figure out the oslist to use */
	LOGR("bgo", bootctl_get_dev(UCLASS_BOOTCTL_OSLIST, &priv->oslist));

	/* figure out the state to use */
	LOGR("bgs", bootctl_get_dev(UCLASS_BOOTCTL_STATE, &priv->state));

	if (priv->opt_labels) {
		LOGR("blo", bootdev_set_order(priv->opt_labels));
	}

	priv->starting = true;

	return 0;
}

/**
 * prepare_for_boot() - Get ready to boot an OS
 *
 * @dev: Bootctrl logic device
 * @osinfo: OS to boot
 * Return: 0 if OK, -ve on error
 */
static int prepare_for_boot(struct udevice *dev, struct osinfo *osinfo)
{
	struct logic_priv *priv = dev_get_priv(dev);
	int ret;

	if (priv->opt_track_success) {
		ret = bc_state_write_bool(priv->state, "recordfail", true);
		if (ret)
			log_warning("Cannot set up recordfail (err=%dE)\n",
				    ret);
	}

	if (priv->opt_persist_state) {
		ret = bc_state_save(priv->state);
		if (ret)
			log_warning("Cannot save state (err=%dE)\n", ret);
		else
			priv->state_saved = true;
	}

	/* devicetree fix-ups go here */

	ret = bootflow_boot(&osinfo->bflow);
	if (ret)
		log_warning("Boot failed (err=%dE)\n", ret);

	return 0;
}

static int logic_poll(struct udevice *dev)
{
	struct logic_priv *priv = dev_get_priv(dev);
	struct osinfo info;
	bool selected;
	bool have_os;
	int ret, seq;

	if (priv->opt_persist_state && !priv->state_load_attempted) {
		int ret;

		/* read in our state */
		priv->state_load_attempted = true;
		ret = bc_state_load(priv->state);
		if (ret)
			log_warning("Cannot load state, starting fresh (err=%dE)\n", ret);
		else
			priv->state_loaded = true;
	}

	if (!priv->ui_shown) {
		LOGR("bds", bc_ui_show(priv->ui));
		priv->ui_shown = true;
	}

	have_os = false;
	if (priv->starting) {
		priv->start_time = get_timer(0);
		if (priv->opt_autoboot) {
			priv->next_countdown = COUNTDOWN_INTERVAL_MS;
			priv->autoboot_remain_s = priv->opt_timeout;
			priv->autoboot_active = true;
		}

		if (priv->opt_default_os) {
			bc_state_read_str(priv->state, "default",
					  &priv->default_os);
		}

		priv->starting = false;
		ret = bc_oslist_first(priv->oslist, &priv->iter, &info);
		if (!ret) {
			priv->scanning = true;
			have_os = true;
		}
	} else if (priv->scanning) {
		ret = bc_oslist_next(priv->oslist, &priv->iter, &info);
		if (!ret)
			have_os = true;
		else
			priv->scanning = false;
	}

	if (have_os)
		LOGR("bda", bc_ui_add(priv->ui, &info));

	if (priv->autoboot_active &&
	    get_timer(priv->start_time) > priv->next_countdown) {
		ulong secs = get_timer(priv->start_time) / 1000;
		priv->autoboot_remain_s = max(priv->opt_timeout - secs, 0ul);
		priv->next_countdown += COUNTDOWN_INTERVAL_MS;
	}

	LOGR("bdr", bc_ui_render(priv->ui));
	LOGR("bdo", bc_ui_poll(priv->ui, &seq, &selected));

	if (!selected && priv->autoboot_active && !priv->autoboot_remain_s &&
	    seq >= 0)
		selected = true;

	if (selected) {
		struct osinfo *os;

		os = alist_getw(&priv->osinfo, seq, struct osinfo);
		LOGR("lpb", prepare_for_boot(dev, os));

		/* boot OS here */

		return -ESHUTDOWN;
	}

	return 0;
}

static int logic_of_to_plat(struct udevice *dev)
{
	struct logic_priv *priv = dev_get_priv(dev);

	ofnode node = ofnode_find_subnode(dev_ofnode(dev), "options");

	priv->opt_persist_state = ofnode_read_bool(node, "persist-state");
	priv->opt_default_os = ofnode_read_bool(node, "default-os");
	ofnode_read_u32(node, "timeout", &priv->opt_timeout);
	priv->opt_skip_timeout = ofnode_read_bool(node,
						"skip-timeout-on-success");
	priv->opt_track_success = ofnode_read_bool(node, "track-success");
	priv->opt_labels = ofnode_read_string(node, "labels");
	priv->opt_autoboot = ofnode_read_bool(node, "autoboot");

	return 0;
}

static int logic_probe(struct udevice *dev)
{
	struct logic_priv *priv = dev_get_priv(dev);

	alist_init_struct(&priv->osinfo, struct osinfo);

	return 0;
}

static struct bc_logic_ops ops = {
	.start	= logic_start,
	.poll	= logic_poll,
};

static const struct udevice_id logic_ids[] = {
	{ .compatible = "bootctl,ubuntu-desktop" },
	{ .compatible = "bootctl,base" },
	{ }
};

U_BOOT_DRIVER(bc_logic) = {
	.name		= "bc_logic",
	.id		= UCLASS_BOOTCTL,
	.of_match	= logic_ids,
	.ops		= &ops,
	.of_to_plat	= logic_of_to_plat,
	.probe		= logic_probe,
	.priv_auto	= sizeof(struct logic_priv),
};
