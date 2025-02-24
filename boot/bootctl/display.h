/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Bootctl display
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#ifndef __bootctl_display_h
#define __bootctl_display_h

struct osinfo;
struct oslist_iter;
struct udevice;

/**
 * struct bc_display_ops - Operations for displays
 */
struct bc_display_ops {
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
	 * @infop: Returns osinfo the user selected
	 * Return 0 if user selected something, -EPIPE if the user tried to quit
	 *	the menu, -EAGAIN if nothin is chosen uet, other -ve on error
	 */
	int (*poll)(struct udevice *dev, struct osinfo **infop);
};

#define bc_display_get_ops(dev)  ((struct bc_display_ops *)(dev)->driver->ops)

/**
 * bc_display_show() - Show the display, ready to accept boot options
 *
 * @dev: Display device
 * Return 0 if OK, -ve on error
 */
int bc_display_show(struct udevice *dev);

/**
 * bc_display_add() - Add an OS to the display, so the user can select it
 *
 * @dev: Display device
 * @info: Information about the OS to display
 * Return 0 if OK, -ve on error
 */
int bc_display_add(struct udevice *dev, struct osinfo *info);

/**
 * bc_display_render() - Render any updates to the display
 *
 * @dev: Display device
 * Return 0 if OK, -ve on error
 */
int bc_display_render(struct udevice *dev);

/**
 * bc_display_poll() - Check for user activity
 *
 * @dev: Display device
 * @infop: Returns osinfo the user selected
 * Return 0 if user selected something, -EPIPE if the user tried to quit
 *	the menu, -EAGAIN if nothin is chosen uet, other -ve on error
 * Return 0 if OK, -ve on error
 */
int bc_display_poll(struct udevice *dev, struct osinfo **infop);

#endif
