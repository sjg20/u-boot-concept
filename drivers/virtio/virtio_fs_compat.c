// SPDX-License-Identifier: GPL-2.0
/*
 * U-Boot Virtio-FS compatibility layer, to allow use with the legacy
 * filesystem-layer
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 *
 */

#define LOG_CATEGORY	UCLASS_VIRTIO

#include <dir.h>
#include <dm.h>
#include <fs.h>
#include <virtio.h>
#include <virtio_fs.h>

static struct udevice *fs_dev;

int virtio_fs_compat_opendir(const char *fname, struct fs_dir_stream **strmp)
{
	struct udevice *dev;
	int ret;

	log_debug("starting fs_dev %p\n", fs_dev);
	log_debug("lookup dev '%s' fname '%s'\n", fs_dev->name, fname);
	ret = fs_lookup_dir(fs_dev, fname, &dev);
	if (ret)
		return log_msg_ret("vld", ret);

	log_debug("open\n");
	ret = dir_open(dev, strmp);
	if (ret)
		return log_msg_ret("vdo", ret);

	return 0;
}

int virtio_fs_compat_readdir(struct fs_dir_stream *strm,
			     struct fs_dirent **dentp)
{
	int ret;

	log_debug("read dev '%s'\n", fs_dev->name);
	ret = dir_read(strm->dev, strm, dentp);
	if (ret)
		return log_msg_ret("vrd", ret);
	log_debug("read done\n");

	return 0;
}

void virtio_fs_compat_closedir(struct fs_dir_stream *strm)
{
	int ret;

	log_debug("close dev '%s'\n", fs_dev->name);
	ret = dir_close(strm->dev, strm);
	if (ret)
		log_err("dir_close() failed: %dE\n", ret);
}

int virtio_fs_compat_probe(struct blk_desc *fs_dev_desc,
			   struct disk_partition *fs_partition)
{
	struct udevice *dev;
	int ret;

	ret = uclass_first_device_err(UCLASS_FS, &dev);
	if (ret) {
		printf("No filesystem (err %dE)\n", ret);
		return ret;
	}
	ret = fs_mount(dev);
	if (ret && ret != -EISCONN) {
		printf("Cannot mount filesystem (err %dE)\n", ret);
		return ret;
	}

	fs_dev = dev;
	log_debug("fs_dev %p\n", fs_dev);

	return 0;
}
