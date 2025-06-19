// SPDX-License-Identifier: GPL-2.0
/*
 * U-Boot Virtio-FS compatibility layer, to allow use with the legacy
 * filesystem-layer
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 *
 */

#define LOG_DEBUG
#define LOG_CATEGORY	UCLASS_VIRTIO

#include <dir.h>
#include <dm.h>
#include <fs.h>
#include <malloc.h>
#include <part.h>
#include <virtio.h>

static struct udevice *fs_dev;

int virtio_fs_opendir(const char *filename, struct fs_dir_stream **strmp)
{
	struct udevice *dev;
	int ret;

	log_debug("lookup %s\n", fs_dev->name);
	ret = fs_lookup_dir(fs_dev, filename, &dev);
	if (ret)
		return log_msg_ret("vld", ret);

	log_debug("open %s\n", dev->name);
	ret = dir_open(dev, strmp);
	if (ret)
		return log_msg_ret("vdo", ret);

	return 0;
}

int virtio_fs_readdir(struct fs_dir_stream *strm, struct fs_dirent **dentp)
{
	int ret;

	log_debug("read %p\n", strm);
	log_debug("read dev %p\n", strm->dev);
	log_debug("read name %s\n", strm->dev->name);

	ret = dir_read(strm->dev, strm, dentp);
	if (ret)
		return log_msg_ret("vrd", ret);
	log_debug("read done\n");

	return 0;
}

void virtio_fs_closedir(struct fs_dir_stream *dirs)
{
}

int virtio_fs_probe(struct blk_desc *fs_dev_desc,
		    struct disk_partition *fs_partition)
{
	struct udevice *dev;
	int ret;

	log_debug("virtio_fs_probe\n");

	ret = uclass_first_device_err(UCLASS_FS, &dev);
	// ret = fs_get_by_name(cmd_arg1(argc, argv), &dev);
	if (ret) {
		printf("No filesystem (err %dE)\n", ret);
		return ret;
	}
	ret = fs_mount(dev);
	if (ret) {
		printf("Cannot mount filesystem (err %dE)\n", ret);
		return ret;
	}

	fs_dev = dev;

	return 0;
}
