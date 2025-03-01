/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Bootctl display
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#ifndef __bootctl_display_h
#define __bootctl_display_h

#include <stdbool.h>

struct osinfo;
struct oslist_iter;
struct udevice;

/**
 * struct bc_ui_ops - Operations for displays
 */
struct bc_ui_ops {
	/**
	 * print() - Show a string on the display
	 *
	 * @dev: Display device
	 * @msg: Message to show
	 * Return 0 if OK, -ve on error
	 */
	int (*print)(struct udevice *dev, const char *msg);

	/**
	 * show() - Show the display, ready to accept boot options
	 *
	 * @dev: Display device
	 * Return 0 if OK, -ve on error
	 */
	int (*show)(struct udevice *dev);

	/**
	 * add() - Add an OS to the display, so the user can select it
	 *
	 * @dev: Display device
	 * @info: Information about the OS to display
	 * Return 0 if OK, -ve on error
	 */
	int (*add)(struct udevice *dev, struct osinfo *info);

	/**
	 * render() - Render any updates to the display
	 *
	 * @dev: Display device
	 * Return 0 if OK, -ve on error
	 */
	int (*render)(struct udevice *dev);

	/**
	 * poll() - Check for user activity
	 *
	 * @dev: Display device
	 * @seqp: Returns the sequence number of the osinfo that is currently
	 *	pointed to/highlighted, or -1 if nothing
	 * @selectedp: Returns true if the user selected an item, else false
	 * Return: 0 if OK, -EPIPE if the user tried to quit the menu, other
	 *	-ve on error
	 */
	int (*poll)(struct udevice *dev, int *seqp, bool *selectedp);
};

#define bc_ui_get_ops(dev)  ((struct bc_ui_ops *)(dev)->driver->ops)

/**
 * bc_ui_show() - Show the display, ready to accept boot options
 *
 * @dev: Display device
 * Return 0 if OK, -ve on error
 */
int bc_ui_show(struct udevice *dev);

/**
 * bc_ui_add() - Add an OS to the display, so the user can select it
 *
 * @dev: Display device
 * @info: Information about the OS to display
 * Return 0 if OK, -ve on error
 */
int bc_ui_add(struct udevice *dev, struct osinfo *info);

/**
 * bc_ui_render() - Render any updates to the display
 *
 * @dev: Display device
 * Return 0 if OK, -ve on error
 */
int bc_ui_render(struct udevice *dev);

/**
 * bc_ui_poll() - Check for user activity
 *
 * @dev: Display device
 * @seqp: Returns the sequence number of the osinfo that is currently
 *	pointed to/highlighted, or -1 if nothing
 * @selectedp: Returns true if the user selected an item, else false
 * Return: 0 if OK, -EPIPE if the user tried to quit the menu, other
 *	-ve on error
 */
int bc_ui_poll(struct udevice *dev, int *seqp, bool *selectedp);

#endif
