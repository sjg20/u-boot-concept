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
#include <file.h>
#include <fs.h>
#include <malloc.h>
#include <virtio.h>
#include <virtio_fs.h>
#include <linux/fuse.h>
#include "fs_internal.h"

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
	struct fs_dirent *dent;
	int ret;

	log_debug("read dev '%s'\n", fs_dev->name);
	dent = malloc(sizeof(struct fs_dirent));
	if (!dent)
		return log_msg_ret("vrD", -ENOMEM);

	ret = dir_read(strm->dev, strm, dent);
	if (ret) {
		free(dent);
		return log_msg_ret("vrd", ret);
	}
	*dentp = dent;
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

int virtio_fs_compat_size(const char *fname, loff_t *sizep)
{
	struct fuse_entry_out out;
	const char *leaf;
	char *subdir;
	int ret;

	log_debug("filename '%s'\n", fname);

	ret = fs_split_path(fname, &subdir, &leaf);
	if (ret)
		return log_msg_ret("vcp", ret);
	log_debug("subdir '%s' leaf '%s'\n", subdir, leaf);

	ret = virtio_fs_lookup_(fs_dev, virtio_fs_get_root(fs_dev), fname,
				&out);
	if (ret)
		return log_msg_ret("vcl", ret);

	log_debug("inode %llx size %llx\n", out.nodeid, out.attr.size);
	*sizep = out.attr.size;

	return 0;
}

int virtio_fs_compat_read(const char *fname, void *buf, loff_t offset,
			  loff_t len, loff_t *actread)
{
	struct udevice *dir, *fil;
	const char *leaf;
	char *subdir;
	long nread;
	int ret;

	log_debug("load '%s'\n", fname);

	ret = fs_split_path(fname, &subdir, &leaf);
	if (ret)
		return log_msg_ret("fcr", ret);
	log_debug("subdir '%s' leaf '%s'\n", subdir, leaf);

	ret = fs_lookup_dir(fs_dev, subdir, &dir);
	if (ret)
		return log_msg_ret("fcl", ret);
	log_debug("dir '%s'\n", dir->name);

	ret = virtio_fs_setup_file(dir, leaf, DIR_O_RDONLY, &fil);
	log_debug("virtio_fs_file_open returned %d\n", ret);
	if (ret)
		return log_msg_ret("fco", ret);

	log_debug("reading file '%s'\n", fil->name);
	nread = file_read_at(fil, buf, offset, len);
	if (nread < 0)
		return log_msg_ret("fco", nread);
	*actread = nread;

	return 0;
}

