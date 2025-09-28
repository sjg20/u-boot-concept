/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Uclass implementation for boot schema
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#ifndef __bootctl_state_h
#define __bootctl_state_h

#include <abuf.h>
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
 * struct bc_state_ops - Operations for the UI
 */
struct bc_state_ops {
	/**
	 * read_bool() - Read a boolean value
	 *
	 * @dev: Device to access
	 * @key: Key to access
	 * @valp: Returns boolean value on success
	 * Return: 0 if OK, or -ve error code
	 */
	int (*read_bool)(struct udevice *dev, const char *key, bool *valp);

	/**
	 * write_bool() - Write a boolean value
	 *
	 * @dev: Device to access
	 * @key: Key to access
	 * @val: Value to write
	 * Return: 0 if OK, or -ve error code
	 */
	int (*write_bool)(struct udevice *dev, const char *key, bool val);

	/**
	 * read_int() - Read an integer value
	 *
	 * This holds a 64-bit integer
	 *
	 * @dev: Device to access
	 * @key: Key to access
	 * @valp: Returns integer value on success
	 * Return: 0 if OK, or -ve error code
	 */
	int (*read_int)(struct udevice *dev, const char *key, long *valp);

	/**
	 * write_str() - Write an integer value
	 *
	 * This holds a 64-bit integer
	 *
	 * @dev: Device to access
	 * @key: Key to access
	 * @val: Value to write
	 * Return: 0 if OK, or -ve error code
	 */
	int (*write_int)(struct udevice *dev, const char *key, long val);

	/**
	 * read_str() - Read a string value
	 *
	 * @dev: Device to access
	 * @key: Key to access
	 * @valp: Returns string value on success
	 * Return: 0 if OK, or -ve error code
	 */
	int (*read_str)(struct udevice *dev, const char *key,
			const char **valp);

	/**
	 * write_str() - Write a string value
	 *
	 * @dev: Device to access
	 * @key: Key to access
	 * @val: Value to write
	 * Return: 0 if OK, or -ve error code
	 */
	int (*write_str)(struct udevice *dev, const char *key, const char *val);

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

	/**
	 * save_to_buf() - Write the current state to a buffer
	 *
	 * The buffer is inited and filled with the contents of the state as it
	 * would be written to a file
	 *
	 * Return: 0 if OK, or -ve error code
	 */
	int (*save_to_buf)(struct udevice *dev, struct abuf *buf);

	/**
	 * clear() - Clear all values
	 *
	 * Return: 0 if OK, or -ve error code
	 */
	int (*clear)(struct udevice *dev);
};

#define bc_state_get_ops(dev)  ((struct bc_state_ops *)(dev)->driver->ops)

/**
 * bc_state_read_bool() - Read a boolean value
 *
 * @dev: Device to access
 * @key: Key to access
 * @valp: Returns boolean value on success
 * Return: 0 if OK, or -ve error code
 */
int bc_state_read_bool(struct udevice *dev, const char *key, bool *valp);

/**
 * bc_state_write_bool() - Write a boolean value
 *
 * Sets the value for a key, overwriting any existing value
 *
 * @dev: Device to access
 * @key: Key to access
 * @val: Value to write
 * Return: 0 if OK, or -ve error code
 */
int bc_state_write_bool(struct udevice *dev, const char *key, bool val);

/**
 * bc_state_read_int() - Read an integer value
 *
 * This holds a 64-bit integer
 *
 * @dev: Device to access
 * @key: Key to access
 * @valp: Returns integer value on success
 * Return: 0 if OK, or -ve error code
 */
int bc_state_read_int(struct udevice *dev, const char *key, long *valp);

/**
 * bc_state_write_str() - Write an integer value
 *
 * This holds a 64-bit integer
 *
 * @dev: Device to access
 * @key: Key to access
 * @val: Value to write
 * Return: 0 if OK, or -ve error code
 */
int bc_state_write_int(struct udevice *dev, const char *key, long val);

/**
 * bc_state_read_str() - Read a string value
 *
 * @dev: Device to access
 * @key: Key to access
 * @valp: Returns string value on success
 * Return: 0 if OK, or -ve error code
 */
int bc_state_read_str(struct udevice *dev, const char *key, const char **valp);

/**
 * bc_state_write_str() - Write a string value
 *
 * @dev: Device to access
 * @key: Key to access
 * @val: Value to write
 * Return: 0 if OK, or -ve error code
 */
int bc_state_write_str(struct udevice *dev, const char *key, const char *val);

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

/**
 * bc_state_save_to_buf() - Write the current state to a buffer
 *
 * The buffer is inited and filled with the contents of the state as it
 * would be written to a file
 *
 * Return: 0 if OK, or -ve error code
 */
int bc_state_save_to_buf(struct udevice *dev, struct abuf *buf);

/**
 * bc_state_clear() - Clear all values
 *
 * Return: 0 if OK, or -ve error code
 */
int bc_state_clear(struct udevice *dev);

#endif
