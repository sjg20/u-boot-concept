/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Implementation of the logic to perform a boot
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#ifndef __bootctl_logic_h
#define __bootctl_logic_h

#include "oslist.h"

struct udevice;

/**
 * struct logic_priv - Information maintained by the boot logic as it works
 *
 * @opt_persist_state: true if state can be preserved across reboots
 * @opt_default_os: true if we record a default OS to boot
 * @opt_track_success: true to track whether the last boot succeeded (made it to
 *	user space)
 * @opt_skip_timeout: true to skip any boot timeout if the last boot succeeded
 * @opt_labels: if non-NULL, a space-separated list of labels which can be used
 *	to boot
 * @opt_autoboot: true to autoboot the default OS after a timeout
 *
 * @ready: true if ready to start scanning for OSes and booting
 * @state_load_attempted: true if we have attempted to load state
 * @state_loaded: true if the state information has been loaded
 * @ui_shown: true if the UI has been shown / written
 * @scanning: true if scanning for new OSes
 * @start_time: monotonic time when the boot started
 * @next_countdown: next monotonic time to check the timeout
 * @autoboot_remain_s: remaining autoboot time in seconds
 * @autoboot_active: true if autoboot is active
 * @default_os: name of the default OS to boot
 * @osinfo: List of OSes to show
 *
 * @iter: oslist iterator, used to find new OSes
 * @selected: index of selected OS in osinfo alist, or -1 if none has been
 *	selected yet
 * @ui: display / console device
 * @oslist: provides OSes to boot
 * @state: provides persistent state
 */
struct logic_priv {
	bool opt_persist_state;
	bool opt_default_os;
	uint opt_timeout;
	bool opt_track_success;
	bool opt_skip_timeout;
	const char *opt_labels;
	bool opt_autoboot;

	bool starting;
	bool state_load_attempted;
	bool state_loaded;
	bool state_saved;
	bool ui_shown;
	bool scanning;
	ulong start_time;
	uint next_countdown;
	uint autoboot_remain_s;
	bool autoboot_active;
	const char *default_os;
	struct alist osinfo;

	struct oslist_iter iter;
	struct udevice *ui;
	struct udevice *oslist;
	struct udevice *state;
};

/**
 * struct bc_logic_ops - Operations related to boot loader
 */
struct bc_logic_ops {
	/**
	 * start() - Start the boot process
	 *
	 * Gets things ready; must be called before poll()
	 *
	 * @dev: Logic device
	 * Return: 0 if OK, or -ve error code
	 */
	int (*start)(struct udevice *dev);

	/**
	 * poll() - Poll the boot process
	 *
	 * Try to progress the boot towards a result
	 *
	 * @dev: Logic device
	 * Return: 0 if OK, or -ve error code
	 */
	int (*poll)(struct udevice *dev);
};

#define bc_logic_get_ops(dev)  ((struct bc_logic_ops *)(dev)->driver->ops)

/**
 * bc_logic_start() - Start the boot process
 *
 * Gets things ready; must be called before poll()
 *
 * @dev: Logic device
 * Return: 0 if OK, or -ve error code
 */
int bc_logic_start(struct udevice *dev);

/**
 * * bc_logic_poll() - Poll the boot process
 *
 * Try to progress the boot towards a result. This handles looking for OSes and
 * presenting them to the user, if any, as well as attempting to boot
 *
 * @dev: Logic device
 * Return: 0 if OK, or -ve error code
 */
int bc_logic_poll(struct udevice *dev);

#endif
