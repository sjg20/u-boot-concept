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
	 * read_bootflow() - read a bootflow for a device
	 *
	 * @dev:	Bootmethod device to use
	 * @bflow:	On entry, provides dev, hwpart, part and method.
	 *	Returns updated bootflow if found
	 * @return 0 if OK, -ve on error
	 */
	int (*read_bootflow)(struct udevice *dev, struct bootflow *bflow);

	/**
	 * read_file() - read a file needed for a bootflow
	 *
	 * Read a file from the same place as the bootflow came from
	 *
	 * @dev:	Bootmethod device to use
	 * @bflow:	Bootflow providing info on where to read from
	 * @file_path:	Path to file (may be absolute or relative)
	 * @addr:	Address to load file
	 * @sizep:	On entry provides the maximum permitted size; on exit
	 *		returns the size of the file
	 * @return 0 if OK, -ENOSPC if the file is too large for @sizep, other
	 *	-ve value if something else goes wrong
	 */
	int (*read_file)(struct udevice *dev, struct bootflow *bflow,
			 const char *file_path, ulong addr, ulong *sizep);

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
 * bootmethod_read_bootflow() - set up a bootflow for a device
 *
 * @dev:	Bootmethod device to check
 * @bflow:	On entry, provides dev, hwpart, part and method.
 *	Returns updated bootflow if found
 * @return 0 if OK, -ve on error
 */
int bootmethod_read_bootflow(struct udevice *dev, struct bootflow *bflow);

/**
 * bootmethod_read_file() - read a file needed for a bootflow
 *
 * Read a file from the same place as the bootflow came from
 *
 * @dev:	Bootmethod device to use
 * @bflow:	Bootflow providing info on where to read from
 * @file_path:	Path to file (may be absolute or relative)
 * @addr:	Address to load file
 * @sizep:	On entry provides the maximum permitted size; on exit
 *		returns the size of the file
 * @return 0 if OK, -ENOSPC if the file is too large for @sizep, other
 *	-ve value if something else goes wrong
 */
int bootmethod_read_file(struct udevice *dev, struct bootflow *bflow,
			 const char *file_path, ulong addr, ulong *sizep);

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
