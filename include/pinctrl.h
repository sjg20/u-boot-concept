/*
 * Copyright (c) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __PINCTRL_H
#define __PINCTRL_H

struct pinctrl_ops {
	/**
	 * request() - Request a particular pinctrl function
	 *
	 * This activates the selected function.
	 *
	 * @dev:	Device to adjust (UCLASS_PINCTRL)
	 * @func:	Function number (driver-specific)
	 * @return 0 if OK, -ve on error
	 */
	int (*request)(struct udevice *dev, int func, int flags);

	/**
	* get_periph_id() - get the peripheral ID for a device
	*
	* This generally looks at the peripheral's device tree node to work
	* out the peripheral ID. The return value is normally interpreted as
	* enum periph_id. so long as this is defined by the platform (which it
	* should be).
	*
	* @dev:		Pinctrl device to use for decoding
	* @periph:	Device to check
	* @return peripheral ID of @periph, or -ENOENT on error
	*/
	int (*get_periph_id)(struct udevice *dev, struct udevice *periph);
};

#define pinctrl_get_ops(dev)	((struct pinctrl_ops *)(dev)->driver->ops)

/**
 * pinctrl_request() - Request a particular pinctrl function
 *
 * @dev:	Device to check (UCLASS_PINCTRL)
 * @func:	Function number (driver-specific)
 * @flags:	Flags (driver-specific)
 * @return 0 if OK, -ve on error
 */
int pinctrl_request(struct udevice *dev, int func, int flags);

/**
 * pinctrl_request_noflags() - Request a particular pinctrl function
 *
 * This is similar to pinctrl_request() but uses 0 for @flags.
 *
 * @dev:	Device to check (UCLASS_PINCTRL)
 * @func:	Function number (driver-specific)
 * @return 0 if OK, -ve on error
 */
int pinctrl_request_noflags(struct udevice *dev, int func);

/**
 * pinctrl_get_periph_id() - get the peripheral ID for a device
 *
 * This generally looks at the peripheral's device tree node to work out the
 * peripheral ID. The return value is normally interpreted as enum periph_id.
 * so long as this is defined by the platform (which it should be).
 *
 * @dev:	Pinctrl device to use for decoding
 * @periph:	Device to check
 * @return peripheral ID of @periph, or -ENOENT on error
 */
int pinctrl_get_periph_id(struct udevice *dev, struct udevice *periph);

#endif
