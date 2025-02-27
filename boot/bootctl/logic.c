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
#include <version.h>
#include <dm/device-internal.h>
#include "logic.h"
#include "oslist.h"
#include "state.h"
#include "ui.h"
#include "util.h"

/**
 * struct logic_priv - Information maintained by the boot logic as it works
 *
 * @have_persistent_state: true if state can be preserved across reboots
 *
 * @ready: true if ready to start scanning for OSes and booting
 * @state_load_attempted: true if we have attempted to load state
 * @state_loaded: true if the state information has been loaded
 * @ui_shown: true if the UI has been shown / written
 * @scanning: true if scanning for new OSes
 *
 * @iter: oslist iterator, used to find new OSes
 * @selected: selected OS, or NULL if none has been selected yet
 * @ui: display / console device
 * @oslist: provides OSes to boot
 * @state: provides persistent state
 */
struct logic_priv {
	bool have_persistent_state;

	bool starting;
	bool state_load_attempted;
	bool state_loaded;
	bool ui_shown;
	bool scanning;

	struct oslist_iter iter;
	struct osinfo *selected;
	struct udevice *ui;
	struct udevice *oslist;
	struct udevice *state;
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

	priv->starting = true;

	return 0;
}

static int logic_poll(struct udevice *dev)
{
	struct logic_priv *priv = dev_get_priv(dev);
	struct osinfo info;
	bool have_os;
	int ret;

	if (priv->have_persistent_state && !priv->state_loaded) {
		int ret;

		/* read in our state */
		priv->state_load_attempted = true;
		ret = bc_state_load(priv->state);
		if (ret == -EINVAL)
			log_debug("Cannot read state, starting fresh (err=%dE)\n", ret);
		else
			priv->state_loaded = true;
	}

	if (!priv->ui_shown) {
		LOGR("bds", bc_ui_show(priv->ui));
		priv->ui_shown = true;
	}

	have_os = false;
	if (priv->starting) {
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

	LOGR("bdr", bc_ui_render(priv->ui));
	LOGR("bdo", bc_ui_poll(priv->ui, &priv->selected));

	if (priv->selected) {
		printf("boot\n");
		/* boot OS here */

		return -ESHUTDOWN;
	}

	return 0;
}

static int logic_of_to_plat(struct udevice *dev)
{
	struct logic_priv *priv = dev_get_priv(dev);

	priv->have_persistent_state = dev_read_bool(dev, "persistent-state");

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
	.priv_auto	= sizeof(struct logic_priv),
};
