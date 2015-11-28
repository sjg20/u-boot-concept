/*
 * Copyright (c) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __pch_h
#define __pch_h

struct pch_ops {
	/**
	 * init() - set up the PCH devices
	 *
	 * This makes sure that all the devices are ready for use. They are
	 * not actually started, just set up so that they can be probed.
	 */
	int (*init)(struct udevice *dev);

	/**
	 * get_sbase() - get the address of SBASE
	 *
	 * @dev:	PCH device to check
	 * @sbasep:	Returns address of SBASE if available, else 0
	 * @return 0 if OK, -ve on error (e.g. there is no SBASE)
	 */
	int (*get_sbase)(struct udevice *dev, ulong *sbasep);

	/**
	 * get_version() - get the PCH version (e.g. 7 or 9)
	 *
	 * @return version, or -1 if unknown
	 */
	int (*get_version)(struct udevice *dev);
};

#define pch_get_ops(dev)        ((struct pch_ops *)(dev)->driver->ops)

/**
 * pch_init() - init a PCH
 *
 * This makes sure that all the devices are ready for use. They are
 * not actually started, just set up so that they can be probed.
 *
 * @dev:	PCH device to init
 * @return 0 if OK, -ve on error
 */
int pch_init(struct udevice *dev);

/**
 * pch_get_sbase() - get the address of SBASE
 *
 * @dev:	PCH device to check
 * @sbasep:	Returns address of SBASE if available, else 0
 * @return 0 if OK, -ve on error (e.g. there is no SBASE)
 */
int pch_get_sbase(struct udevice *dev, ulong *sbasep);

/**
 * pch_get_version() - get the PCH version (e.g. 7 or 9)
 *
 * @return version, or -ve if unknown/error
 */
int pch_get_version(struct udevice *dev);

#endif
