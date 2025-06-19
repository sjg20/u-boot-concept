// SPDX-License-Identifier: GPL-2.0
/*
 * Implementation of a directory on a filesystem
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <dm.h>
#include <dir.h>

int dir_open(struct udevice *dev, struct fs_dir_stream **dirsp)
{
	struct dir_ops *ops = dir_get_ops(dev);

	return ops->open(dev, dirsp);
}

UCLASS_DRIVER(dir) = {
	.name	= "dir",
	.id	= UCLASS_DIR,
};
