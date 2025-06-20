/* SPDX-License-Identifier: GPL-2.0 */
/*
 * U-Boot Filesystem directory
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#ifndef __DIR_H
#define __DIR_H

struct fs_dirent;
struct fs_dir_stream;
struct udevice;

struct dir_ops {
	int (*open)(struct udevice *dev, struct fs_dir_stream **strmp);

	int (*read)(struct udevice *dev, struct fs_dir_stream *strm,
		    struct fs_dirent **dentp);

	int (*close)(struct udevice *dev, struct fs_dir_stream *strm);
};

/* Get access to a directory's operations */
#define dir_get_ops(dev)		((struct dir_ops *)(dev)->driver->ops)

/** Open a directory to read its contents */
int dir_open(struct udevice *dev, struct fs_dir_stream **strmp);

int dir_read(struct udevice *dev, struct fs_dir_stream *strm,
	     struct fs_dirent **dentp);

int dir_close(struct udevice *dev, struct fs_dir_stream *strm);

#endif
