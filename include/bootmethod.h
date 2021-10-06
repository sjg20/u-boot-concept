/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __bootmethod_h
#define __bootmethod_h

struct bootflow;

/**
 * struct bootmethod_ops - Operations for the Platform Controller Hub
 *
 * Consider using ioctl() to add rarely used or driver-specific operations.
 */
struct bootmethod_ops {
	/**
	 * setup() - set up a bootflow for a device
	 *
	 * @dev:	Bootmethod device to check
	 * @bflow:	On entry, provides dev, hwpart, part and method.
	 *	Returns updated bootflow if found
	 * @return 0 if OK, -ve on error
	 */
	int (*setup)(struct udevice *dev, struct bootflow *bflow);

	/**
	 * boot() - boot a bootflow
	 *
	 * @dev:	Bootmethod device to boot
	 * @bflow:	Bootflow to boot
	 * @return does not return on success, since it should boot the
	 *	Operating Systemn. Returns -EFAULT if that fails, other -ve on
	 *	other error
	 */
	int (*boot)(struct udevice *dev, struct bootflow *bflow);
};

#define bootmethod_get_ops(dev)  ((struct bootmethod_ops *)(dev)->driver->ops)

/**
 * bootmethod_setup() - set up a bootflow for a device
 *
 * @dev:	Bootmethod device to check
 * @bflow:	On entry, provides dev, hwpart, part and method.
 *	Returns updated bootflow if found
 * @return 0 if OK, -ve on error
 */
int bootmethod_setup(struct udevice *dev, struct bootflow *bflow);

/**
 * bootmethod_boot() - boot a bootflow
 *
 * @dev:	Bootmethod device to boot
 * @bflow:	Bootflow to boot
 * @return does not return on success, since it should boot the
 *	Operating Systemn. Returns -EFAULT if that fails, other -ve on
 *	other error
 */
int bootmethod_boot(struct udevice *dev, struct bootflow *bflow);

#endif
