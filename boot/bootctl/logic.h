/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Implementation of the logic to perform a boot
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#ifndef __bootctl_logic_h
#define __bootctl_logic_h

struct udevice;

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
