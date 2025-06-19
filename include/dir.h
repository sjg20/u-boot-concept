/* SPDX-License-Identifier: GPL-2.0 */
/*
 * U-Boot Filesystem directory
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#ifndef __DIR_H
#define __DIR_H

struct fs_dir_stream;
struct udevice;

struct dir_ops {
	int (*open)(struct udevice *dev, struct fs_dir_stream **dirsp);
};

/* Get access to a directory's operations */
#define dir_get_ops(dev)		((struct dir_ops *)(dev)->driver->ops)

/** Open a directory to read its contents */
int dir_open(struct udevice *dev, struct fs_dir_stream **dirsp);

#endif
