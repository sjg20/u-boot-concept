/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

/* Interface for access the firmware image in storage (e.g. SPI flash) */

#ifndef CROS_FWSTORE_H_
#define CROS_FWSTORE_H_

#include <dm/of_extra.h>

/**
 * These read or write [count] bytes starting from [offset] of storage into or
 * from the [buf].
 *
 * @return 0 if it succeeds, non-zero if it fails
 */
struct cros_fwstore_ops {
	/**
	 * read() - read data
	 *
	 * @dev:	Device to read from
	 * @offset:	Offset within device to read from in bytes
	 * @count:	Number of bytes to read
	 * @buf:	Buffer to place data
	 * @return 0 if OK, -ve on error
	 */
	int (*read)(struct udevice *dev, ulong offset, ulong count, void *buf);

	/**
	 * write() - write data
	 *
	 * @dev:	Device to write to
	 * @offset:	Offset within device to write to in bytes
	 * @count:	Number of bytes to write
	 * @buf:	Buffer containg data to write
	 * @return 0 if OK, -ve on error
	 */
	int (*write)(struct udevice *dev, ulong offset, ulong count, void *buf);

	/**
	 * sw_wp_enabled() - see if software write protect is enabled
	 *
	 * @dev:	Device to check
	 * @return	1 if sw wp is enabled, 0 if disabled, -ve on error
	 */
	int (*sw_wp_enabled)(struct udevice *dev);
};

#define cros_fwstore_get_ops(dev) \
		((struct cros_fwstore_ops *)(dev)->driver->ops)

/**
 * cros_fwstore_read() - read data
 *
 * @dev:	Device to read from
 * @offset:	Offset within device to read from in bytes
 * @count:	Number of bytes to read
 * @buf:	Buffer to place data
 * @return 0 if OK, -ve on error
 */
int cros_fwstore_read(struct udevice *dev, int offset, int count, void *buf);

/**
 * cros_fwstore_write() - write data
 *
 * @dev:	Device to write to
 * @offset:	Offset within device to write to in bytes
 * @count:	Number of bytes to write
 * @buf:	Buffer containg data to write
 * @return 0 if OK, -ve on error
 */
int cros_fwstore_write(struct udevice *dev, int offset, int count, void *buf);

/**
 * cros_fwstore_sw_wp_enabled() - see if software write protect is enabled
 *
 * @dev:	Device to check
 * @return	1 if sw wp is enabled, 0 if disabled, -ve on error
 */
int cros_fwstore_sw_wp_enabled(struct udevice *dev);

/**
 * fwstore_reader_setup() - Set up an existing reader for SPI flash
 *
 * This sets the platform data for the reader device so that it can operate
 * correctly. The device should be inactive. It is not probed by this function.
 *
 * @dev: Device to set up
 * @offset: Start offset in flash to allow the device to access
 * @size: Size of region to allow the device to access
 */
void fwstore_reader_setup(struct udevice *dev, int offset, int size);

int fwstore_reader_restrict(struct udevice *dev, int offset, int size);

int fwstore_reader_size(struct udevice *dev);

int fwstore_get_reader_dev(struct udevice *fwstore, int offset, int size,
			   struct udevice **devp);

/**
 * fwstore_load_image() - Allocate and load an image from the firmware store
 *
 * This allocates memory for the image and returns a pointer to it.
 *
 * @dev: Device to load from
 * @entry:	Flash entry to load (provides offset, size, compression,
 *		uncompressed size)
 * @imagep:	Returns a pointer to the data (must be freed by caller)
 * @image_sizep: Returns image size
 * @return 0 if OK, -ENOENT if the image has a zero size, -ENOMEM if there is
 *	not enough memory for the buffer, other error on read failre
 */
int fwstore_load_image(struct udevice *dev, struct fmap_entry *entry,
		       u8 **imagep, int *image_sizep);

int cros_fwstore_read_decomp(struct udevice *dev, struct fmap_entry *entry,
			     void *buf, int buf_size);

#endif /* CROS_FWSTORE_H_ */
