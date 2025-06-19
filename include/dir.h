/* SPDX-License-Identifier: GPL-2.0 */
/*
 * U-Boot Filesystem directory
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#ifndef __DIR_H
#define __DIR_H

struct udevice;

struct dir_ops {
	int (*open)(struct udevice *dev, const char *path);
};

/* Get access to a directory's operations */
#define fs_get_ops(dev)		((struct fs_ops *)(dev)->driver->ops)

/** Open a directory to read its contents */
int dir_open(struct udevice *dev, const char *dirname);

#endif
