/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Uclass implementation for boot schema
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#ifndef __bootctl_h
#define __bootctl_h

#include <bootflow.h>

struct udevice;

/**
 * enum bootctl_type - types of bootctl drivers
 *
 * Each bootctl driver has a type, indicating what it is used for during the
 * boot process.
 *
 * An alternative would be to have a separate uclass for each type, but that
 * might be 10-20 uclasses, which is quite a lot.
 *
 * @BOOTCTLT_CORE: Core logic
 * @BOOTCTLT_DISPLAY: Display of information to the user
 * @BOOTCTLT_OSLIST: Provides a list of Operating Systems to boot
 */
enum bootctl_type {
	BOOTCTLT_CORE,
	BOOTCTLT_DISPLAY,
	BOOTCTLT_OSLIST,

	BOOTCTLT_COUNT,
};

/**
 * struct bootctl_uc_plat - information the uclass keeps about each bootctl
 *
 * @desc: A long description of the bootctl
 */
struct bootctl_uc_plat {
	const char *desc;
	enum bootctl_type type;
};

/**
 * bootctl_get_dev() - Get a device of a given type
 *
 * @type: Type to search for
 * @devp: Return the device found, on success
 * Return: 0 on success, or -ve error
 */
int bootctl_get_dev(enum bootctl_type type, struct udevice **devp);

/**
 * bootctl_run() - Run a boot
 *
 * Return: 0 on success, or -ve error
 */
int bootctl_run(void);

#endif
