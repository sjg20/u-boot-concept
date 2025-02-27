/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Uclass implementation for boot schema
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#ifndef __bootctl_state_h
#define __bootctl_state_h

#include <alist.h>

struct udevice;

/**
 * struct state - State information which can be read and written
 *
 * @bflow: Bootflow for this OS
 */
struct bc_state {
	struct alist subnodes;
};

/**
 * struct bc_state_ops - Operations for displays
 */
struct bc_state_ops {
	/**
	 * read_bool() - Read a boolean value
	 *
	 * @dev: Device to access
	 * @prop: Property to access
	 * @valp: Returns boolean value on success
	 * Return: 0 if OK, or -ve error code
	 */
	int (*read_bool)(struct udevice *dev, const char *prop, bool *valp);

	/**
	 * write_bool() - Write a boolean value
	 *
	 * @dev: Device to access
	 * @prop: Property to access
	 * @val: Value to write
	 * Return: 0 if OK, or -ve error code
	 */
	int (*write_bool)(struct udevice *dev, const char *prop, bool val);

	/**
	 * load() - Read in the current state
	 *
	 * Return: 0 if OK, or -ve error code
	 */
	int (*load)(struct udevice *dev);

	/**
	 * save() - Write out the current state
	 *
	 * Return: 0 if OK, or -ve error code
	 */
	int (*save)(struct udevice *dev);
};

#define bc_state_get_ops(dev)  ((struct bc_state_ops *)(dev)->driver->ops)

/**
 * bc_state_read_bool() - Read a boolean value
 *
 * @dev: Device to access
 * @prop: Property to access
 * @valp: Returns boolean value on success
 * Return: 0 if OK, or -ve error code
 */
int bc_state_read_bool(struct udevice *dev, const char *prop, bool *valp);

/**
 * bc_state_write_bool() - Write a boolean value
 *
 * @dev: Device to access
 * @prop: Property to access
 * @val: Value to write
 * Return: 0 if OK, or -ve error code
 */
int bc_state_write_bool(struct udevice *dev, const char *prop, bool val);

/**
  * bc_state_load() - Load state from a file
  *
  * @dev: Device to access
  * Return: 0 if OK, or -ve error code
  */
int bc_state_load(struct udevice *dev);

/**
  * bc_state_save() - Save state to a file
  *
  * @dev: Device to access
  * Return: 0 if OK, or -ve error code
  */
int bc_state_save(struct udevice *dev);

#endif
