// SPDX-License-Identifier: GPL-2.0
/*
 * U-Boot Virtio-FS compatibility layer, to allow use with the legacy
 * filesystem-layer
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 *
 */

#include <dm.h>
#include <fs.h>
#include <part.h>
#include <virtio.h>

int virtio_fs_opendir(const char *filename, struct fs_dir_stream **dirsp)
{
}

int virtio_fs_readdir(struct fs_dir_stream *dirs, struct fs_dirent **dentp)
{
}

void virtio_fs_closedir(struct fs_dir_stream *dirs)
{
}

int virtio_fs_probe(struct blk_desc *fs_dev_desc,
		    struct disk_partition *fs_partition)
{
	struct udevice *dev;
	int ret;

	ret = uclass_first_device_err(UCLASS_FS, &dev);
	// ret = fs_get_by_name(cmd_arg1(argc, argv), &dev);
	if (ret) {
		printf("No filesystem (err %dE)\n", ret);

		return -ENOENT;
	}

	return 0;
}
