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

struct fs_ops {
	int (*ls)(struct udevice *dev, const char *path);
};

/* Get access to a filesystem's operations */
#define fs_get_ops(dev)		((struct fs_ops *)(dev)->driver->ops)

/** List a directory */
int fs_ls(struct udevice *dev, const char *dirname);

/*
int fs_read(struct udevice *dev, const char *dirname);
*/

int fs_get_by_name(const char *name, struct udevice **devp);

#endif
