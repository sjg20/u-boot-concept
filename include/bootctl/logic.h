/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Implementation of the logic to perform a boot
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#ifndef __bootctl_logic_h
#define __bootctl_logic_h

#include <bootctl/oslist.h>

struct udevice;

/**
 * struct logic_priv - Information maintained by the boot logic as it works
 *
 * @opt_persist_state: true if state can be preserved across reboots
 * @opt_default_os: true if we record a default OS to boot
 * @opt_timeout: boot timeout in seconds
 * @opt_skip_timeout: true to skip any boot timeout if the last boot succeeded
 * @opt_track_success: true to track whether the last boot succeeded (made it to
 * user space)
 * @opt_labels: if non-NULL, a space-separated list of bootstd labels which can
 * be used to boot
 * @opt_autoboot: true to autoboot the default OS after a timeout
 * @opt_measure: true to measure loaded images, etc.
 *
 * @state_loaded: true if the state information has been loaded
 * @scanning: true if scanning for new OSes
 * @start_time: monotonic time when the boot started
 * @next_countdown: next monotonic time to check the timeout
 * @autoboot_remain_s: remaining autoboot time in seconds
 * @autoboot_active: true if autoboot is active
 * @default_os: name of the default OS to boot
 * @osinfo: List of OSes to show
 * @refresh: true if we need to refresh the UI because something has changed
 *
 * @iter: oslist iterator, used to find new OSes
 * @selected: index of selected OS in osinfo alist, or -1 if none has been
 *	selected yet
 * @meas: TPM-measurement device
 * @oslist: provides OSes to boot; we iterate through each osinfo driver to find
 * all OSes
 * @state: provides persistent state
 * @ui: provides a visual boot menu on a display / console device
 */
struct logic_priv {
	bool opt_persist_state;
	bool opt_default_os;
	uint opt_timeout;
	bool opt_skip_timeout;
	bool opt_track_success;
	const char *opt_labels;
	bool opt_autoboot;
	bool opt_measure;

	bool state_loaded;
	bool state_saved;
	bool scanning;
	ulong start_time;
	uint next_countdown;
	uint autoboot_remain_s;
	bool autoboot_active;
	const char *default_os;
	struct alist osinfo;
	bool refresh;

	struct oslist_iter iter;
	struct udevice *meas;
	struct udevice *oslist;
	struct udevice *state;
	struct udevice *ui;
};

/**
 * struct bc_logic_ops - Operations related to boot loader
 */
struct bc_logic_ops {
	/**
	 * prepare() - Prepare the components needed for the boot
	 *
	 * This sets up the various device, like ui and oslist
	 *
	 * This must be called before start()
	 *
	 * @dev: Logic device
	 * Return: 0 if OK, or -ve error code
	 */
	int (*prepare)(struct udevice *dev);

	/**
	 * start() - Start the boot process
	 *
	 * Gets things ready, shows the UI, etc.
	 *
	 * This pust be called before poll()
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
	 * This should be called repeatedly until it either boots and OS (iwc
	 * it won't return) or returns an error code
	 *
	 * @dev: Logic device
	 * Return: does not return if OK, -ESHUTDOWN if something went wrong
	 */
	int (*poll)(struct udevice *dev);
};

#define bc_logic_get_ops(dev)  ((struct bc_logic_ops *)(dev)->driver->ops)

/**
 * bc_logic_prepare() - Prepare the components needed for the boot
 *
 * This sets up the various device, like ui and oslist
 *
 * This must be called before start()
 *
 * @dev: Logic device
 * Return: 0 if OK, or -ve error code
 */
int bc_logic_prepare(struct udevice *dev);

/**
 * bc_logic_start() - Start the boot process
 *
 * Gets things ready, shows the UI, etc.
 *
 * This pust be called before poll()
 *
 * @dev: Logic device
 * Return: 0 if OK, or -ve error code
 */
int bc_logic_start(struct udevice *dev);

/**
 * bc_logic_poll() - Poll the boot process
 *
 * Try to progress the boot towards a result
 *
 * This should be called repeatedly until it either boots and OS (iwc
 * it won't return) or returns an error code
 *
 * @dev: Logic device
 * Return: does not return if OK, -ESHUTDOWN if something went wrong
 */
int bc_logic_poll(struct udevice *dev);

#endif
