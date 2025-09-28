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
 * struct bootctl_uc_plat - information the uclass keeps about each bootctl
 *
 * @desc: A long description of the bootctl
 */
struct bootctl_uc_plat {
	const char *desc;
};

/**
 * bootctl_get_dev() - Get a device of a given type
 *
 * @type: Type to search for
 * @devp: Return the device found, on success
 * Return: 0 on success, or -ve error
 */
int bootctl_get_dev(enum uclass_id type, struct udevice **devp);

/**
 * bootctl_run() - Run a boot
 *
 * Return: 0 on success, or -ve error
 */
int bootctl_run(void);

#endif
