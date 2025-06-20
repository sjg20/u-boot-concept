// SPDX-License-Identifier: GPL-2.0
/*
 * Implementation of a directory on a filesystem
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY	UCLASS_DIR

#include <dm.h>
#include <dir.h>

int dir_open(struct udevice *dev, struct fs_dir_stream **strmp)
{
	struct dir_ops *ops = dir_get_ops(dev);

	return ops->open(dev, strmp);
}

int dir_read(struct udevice *dev, struct fs_dir_stream *strm,
	     struct fs_dirent **dentp)
{
	struct dir_ops *ops = dir_get_ops(dev);

	log_debug("dir_read %s\n", dev->name);
	log_debug("dir_read %p\n", ops);

	return ops->read(dev, strm, dentp);
}

int dir_close(struct udevice *dev, struct fs_dir_stream *strm)
{
	struct dir_ops *ops = dir_get_ops(dev);

	log_debug("dir_close %s\n", dev->name);

	return ops->close(dev, strm);
}

UCLASS_DRIVER(dir) = {
	.name	= "dir",
	.id	= UCLASS_DIR,
};
