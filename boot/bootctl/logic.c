// SPDX-License-Identifier: GPL-2.0+
/*
 * Implementation of the logic to perform a boot
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#define LOG_CATEGORY	UCLASS_BOOTCTL

#include <bootctl.h>
#include <dm.h>
#include <log.h>
#include <time.h>
#include <version.h>
#include <bootctl/logic.h>
#include <bootctl/measure.h>
#include <bootctl/oslist.h>
#include <bootctl/state.h>
#include <bootctl/ui.h>
#include <bootctl/util.h>
#include <dm/device-internal.h>

enum {
	COUNTDOWN_INTERVAL_MS	= 1000,	/* inteval between autoboot updates */
};

static int logic_prepare(struct udevice *dev)
{
	struct logic_priv *priv = dev_get_priv(dev);
	int ret;

	/* figure out the UI to use */
	ret = bootctl_get_dev(UCLASS_BOOTCTL_UI, &priv->ui);
	if (ret)
		return log_msg_ret("bgd", ret);

	/* figure out the measurement to use */
	if (priv->opt_measure) {
		ret = bootctl_get_dev(UCLASS_BOOTCTL_MEASURE,
					    &priv->meas);
		if (ret) {
			log_err("Measurement required but failed (err=%dE)\n",
				ret);
			return log_msg_ret("bgs", ret);
		}
	}

	/* figure out at least one oslist driver to use */
	ret = uclass_first_device_err(UCLASS_BOOTCTL_OSLIST, &priv->oslist);
	if (ret)
		return log_msg_ret("bgo", ret);

	/* figure out the state to use */
	ret = bootctl_get_dev(UCLASS_BOOTCTL_STATE, &priv->state);
	if (ret)
		return log_msg_ret("bgs", ret);

	if (priv->opt_labels) {
		ret = bootdev_set_order(priv->opt_labels);
		if (ret)
			return log_msg_ret("blo", ret);
	}


	return 0;
}

static int logic_start(struct udevice *dev)
{
	struct logic_priv *priv = dev_get_priv(dev);
	int ret;

	if (priv->opt_persist_state) {
		int ret;

		/* read in our state */
		ret = bc_state_load(priv->state);
		if (ret)
			log_warning("Cannot load state, starting fresh (err=%dE)\n", ret);
		else
			priv->state_loaded = true;
	}

	ret = bc_ui_show(priv->ui);
	if (ret) {
		log_error("Cannot show display (err=%dE)\n", ret);
		return log_msg_ret("bds", ret);
	}

	priv->start_time = get_timer(0);
	if (priv->opt_autoboot) {
		priv->next_countdown = COUNTDOWN_INTERVAL_MS;
		priv->autoboot_remain_s = priv->opt_timeout;
		priv->autoboot_active = true;
	}

	if (priv->opt_default_os)
		bc_state_read_str(priv->state, "default", &priv->default_os);

	if (priv->opt_measure) {
		ret = bc_measure_start(priv->meas);
		if (ret)
			return log_msg_ret("pme", ret);
	}

	/* start scanning for OSes */
	bc_oslist_setup_iter(&priv->iter);
	priv->scanning = true;

	return 0;
}

/**
 * prepare_for_boot() - Get ready to boot an OS
 *
 * Intended to include at least:
 *   - A/B/recovery logic
 *   - persist the state
 *   - devicetree fix-up
 *   - measure images
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

	/* measure loaded images */
	if (priv->opt_measure) {
		struct alist result;

		ret = bc_measure_process(priv->meas, osinfo, &result);
		if (ret)
			return log_msg_ret("pbp", ret);
		show_measures(&result);

		/* TODO: pass these measurements on to OS */
	}

	return 0;
}

/**
 * read_images() - Read all the images needed to boot an OS
 *
 * @dev: Bootctrl logic device
 * @osinfo: OS we intend to boot
 * Return: 0 if OK, -ve on error
 */
static int read_images(struct udevice *dev, struct osinfo *osinfo)
{
	struct bootflow *bflow = &osinfo->bflow;
	int ret;

	ret = bootflow_read_all(bflow);
	if (ret)
		return log_msg_ret("rea", ret);
	log_debug("Images read: %d\n", bflow->images.count);

	return 0;
}

static int logic_poll(struct udevice *dev)
{
	struct logic_priv *priv = dev_get_priv(dev);
	struct osinfo info;
	bool selected;
	int ret, seq;

	/* scan for the next OS, if any */
	if (priv->scanning) {
		ret = bc_oslist_next(priv->oslist, &priv->iter, &info);
		if (!ret) {
			ret = bc_ui_add(priv->ui, &info);
			if (ret)
				return log_msg_ret("bda", ret);
			priv->refresh = true;
		} else {
			/* No more OSes from this driver, try the next */
			ret = uclass_next_device_err(&priv->oslist);
			if (ret)
				priv->scanning = false;
			else
				memset(&priv->iter, '\0',
				       sizeof(struct oslist_iter));
		}
	}

	if (priv->autoboot_active &&
	    get_timer(priv->start_time) > priv->next_countdown) {
		ulong secs = get_timer(priv->start_time) / 1000;

		priv->autoboot_remain_s = secs >= priv->opt_timeout ? 0 :
			max(priv->opt_timeout - secs, 0ul);
		priv->next_countdown += COUNTDOWN_INTERVAL_MS;
		priv->refresh = true;
	}

	if (priv->refresh) {
		ret = bc_ui_render(priv->ui);
		if (ret)
			return log_msg_ret("bdr", ret);
		priv->refresh = false;
	}

	ret = bc_ui_poll(priv->ui, &seq, &selected);
	if (ret < 0)
		return log_msg_ret("bdo", ret);
	else if (ret)
		priv->refresh = true;

	if (!selected && priv->autoboot_active && !priv->autoboot_remain_s &&
	    seq >= 0) {
		log_info("Selecting %d due to timeout\n", seq);
		selected = true;
	}

	if (selected) {
		struct osinfo *os;

		os = alist_getw(&priv->osinfo, seq, struct osinfo);
		log_info("Selected %d: %s\n", seq, os->bflow.os_name);

		/*
		 * try to read the images first; some methods don't support
		 * this
		 */
		ret = read_images(dev, os);
		if (ret && ret != -ENOSYS) {
			if (ret)
				return log_msg_ret("lri", ret);
		}
		ret = prepare_for_boot(dev, os);
		if (ret)
			return log_msg_ret("lpb", ret);

		/* debugging */
		// return -ESHUTDOWN;

		/* boot OS */
		ret = bootflow_boot(&os->bflow);
		if (ret)
			log_warning("Boot failed (err=%dE)\n", ret);

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
	priv->opt_measure = ofnode_read_bool(node, "measure");

	return 0;
}

static int logic_probe(struct udevice *dev)
{
	struct logic_priv *priv = dev_get_priv(dev);

	alist_init_struct(&priv->osinfo, struct osinfo);

	return 0;
}

static struct bc_logic_ops ops = {
	.prepare	= logic_prepare,
	.start		= logic_start,
	.poll		= logic_poll,
};

static const struct udevice_id logic_ids[] = {
	{ .compatible = "bootctl,ubuntu-desktop" },
	{ .compatible = "bootctl,logic" },
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
