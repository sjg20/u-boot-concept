/* SPDX-License-Identifier: GPL-2.0 */
/*
 * U-Boot Filesystem layer
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#ifndef __FS_H
#define __FS_H

#include <fs_common.h>

struct udevice;

enum {
	/* Maximum length of the filesystem name */
	FS_MAX_NAME_LEN		= 128,
};

struct fs_plat {
	char name[FS_MAX_NAME_LEN];
};

/**
 * struct fs_priv - Private information for the FS devices
 *
 * @mounted: true if mounted
 */
struct fs_priv {
	bool mounted;
};

struct fs_ops {
	/**
	 * mount() - Mount the filename
	 *
	 * @dev: Filesystem device
	 * Return 0 if OK, -EISCONN if already mounted, other -ve on error
	 */
	int (*mount)(struct udevice *dev);

	/**
	 * unmount() - Unmount the filename
	 *
	 * @dev: Filesystem device
	 * Return 0 if OK, -ENOTCONN if not mounted, other -ve on error
	 */
	int (*unmount)(struct udevice *dev);

	/**
	 * lookup_dir() - Look up a directory on a filesystem
	 *
	 * @dev: Filesystem device
	 * @path: Path to look up, empty for the root
	 * @dirp: Returns associated directory device, creating if necessary
	 */
	int (*lookup_dir)(struct udevice *dev, const char *path,
			  struct udevice **dirp);
};

/* Get access to a filesystem's operations */
#define fs_get_ops(dev)		((struct fs_ops *)(dev)->driver->ops)

/** List a directory */
int fs_lookup_dir(struct udevice *dev, const char *path, struct udevice **dirp);

/*
int fs_read(struct udevice *dev, const char *dirname);
*/

int fs_get_by_name(const char *name, struct udevice **devp);

#endif
