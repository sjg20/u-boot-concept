// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for a directory on a filesystem
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <dm.h>
#include <dir.h>

struct dir_priv {
};

static int simple_dir_open(struct udevice *dev, struct fs_dir_stream **dirsp)
{
	return 0;
}

static struct dir_ops simple_dir_ops = {
	.open	= simple_dir_open,
};

static const struct udevice_id dir_ids[] = {
	{ .compatible = "u-boot,directory" },
	{ }
};

U_BOOT_DRIVER(simple_dir) = {
	.name	= "dir",
	.id	= UCLASS_DIR,
	.of_match = dir_ids,
	.ops	= &simple_dir_ops,
	.priv_auto	= sizeof(struct dir_priv),
	.flags	= DM_FLAG_ACTIVE_DMA,
};
