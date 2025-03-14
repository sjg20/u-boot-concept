/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Implementation of measurement of loaded images and the like
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#ifndef __bootctl_measure_h
#define __bootctl_measure_h

#include <alist.h>

struct osinfo;
struct udevice;

/**
 * enum measure_t - Types of measurement supported
 *
 * @BCMEAST_OS: OS image loaded from storage
 * @BCMEAST_INITRD: Initial ramdisk loaded from storage
 * @BCMEAST_CMDLINE: Command-line arguments constructed for the OS
 * @BCMEAST_FDT: Flattened device tree for use by the OS
 */
enum measure_t {
	BCMEAST_IMAGE,
	BCMEAST_CMDLINE,
	BCMEAST_FDT,
};

/**
 * struct measure_info - Information about a particular measurement
 *
 * TODO: Add more details about the measurement
 *
 * @img: Image which was measured
 */
struct measure_info {
	const struct bootflow_img *img;
};

/**
 * struct bc_measure_ops - Operations for measurement, e.g. with a TPM
 */
struct bc_measure_ops {
	/**
	 * start() - Start up ready for measurement
	 *
	 * Sets up the TPM log and starts the TPM
	 *
	 * @dev: Device to access
	 * Return: 0 if OK, or -ve error code
	 */
	int (*start)(struct udevice *dev);

	/**
	 * process() - Measurement of images, etc.
	 *
	 * Process the required measurements to produce results
	 *
	 * @dev: Device to access
	 * @osinfo: OS being booting
	 * @result: Inits and returns a list of struct measure_info records
	 * Return: 0 if OK, or -ve error code
	 */
	int (*process)(struct udevice *dev, const struct osinfo *osinfo,
		       struct alist *result);
};

#define bc_measure_get_ops(dev)  ((struct bc_measure_ops *)(dev)->driver->ops)

/**
 * bc_measure_start() - Start up ready for measurement
 *
 * Sets up the TPM log and starts the TPM
 *
 * @dev: Device to access
 * Return: 0 if OK, or -ve error code
 */
int bc_measure_start(struct udevice *dev);

/**
 * bc_measure_process() - Measurement of images, etc.
 *
 * Processes the required measurements to produce results
 *
 * @dev: Device to access
 * @osinfo: OS being booting
 * @result: Inits and returns a list of struct measure_info records
 * Return: 0 if OK, or -ve error code
 */
int bc_measure_process(struct udevice *dev, const struct osinfo *osinfo,
		       struct alist *result);

#endif
