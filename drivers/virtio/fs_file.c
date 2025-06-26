// SPDX-License-Identifier: GPL-2.0
/*
 * U-Boot Virtio-FS files
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 *
 * Supports access to files in virtio-fs
 */

#define LOG_CATEGORY	UCLASS_VIRTIO

#include <dm.h>
#include <file.h>
#include <fs.h>
#include <malloc.h>
#include <virtio_fs.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <linux/fuse.h>
#include "fs_internal.h"

/**
 * struct file_priv - Information about a virtio file
 *
 * @flags: Open-mode flags
 * @backing_id: Backing ID for the file
 * @fh: Unique filehandle for the file
 * @size: Size of the file
 */
struct file_priv {
	u64 nodeid;
	enum dir_open_flags_t flags;
	u64 fh;
};

static ssize_t virtio_fs_read_iter(struct udevice *dev, struct iov_iter *iter,
				   loff_t pos)
{
	struct file_priv *priv = dev_get_priv(dev);
	struct udevice *dir = dev_get_parent(dev);
	struct udevice *fsdev = dev_get_parent(dir);
	ssize_t ret;

	log_debug("start dev '%s' len %lx\n", dev->name, iter->count);
	ret = virtio_fs_read(fsdev, priv->nodeid, priv->fh, pos,
			     iter_iov_ptr(iter), iter_iov_avail(iter));
	if (ret < 0)
		return log_msg_ret("vfr", ret);
	iter_advance(iter, ret);
	log_debug("read %lx bytes\n", ret);

	return ret;
}

static int virtio_fs_file_remove(struct udevice *dev)
{
	struct virtio_fs_dir_priv *dir_priv = dev_get_priv(dev);

	if (*dir_priv->path) {
		int ret;

		ret = virtio_fs_forget(dev, dir_priv->inode);
		if (ret)
			return log_msg_ret("vfr", ret);
	}

	return 0;
}

static struct file_ops virtio_fs_file_ops = {
	.read_iter	= virtio_fs_read_iter,
};

static const struct udevice_id file_ids[] = {
	{ .compatible = "virtio-fs,file" },
	{ }
};

U_BOOT_DRIVER(virtio_fs_file) = {
	.name	= "virtio_fs_file",
	.id	= UCLASS_FILE,
	.of_match = file_ids,
	.remove	= virtio_fs_file_remove,
	.ops	= &virtio_fs_file_ops,
	.priv_auto	= sizeof(struct file_priv),
	.flags	= DM_FLAG_ACTIVE_DMA,
};

int virtio_fs_setup_file(struct udevice *dir, const char *leaf,
			enum dir_open_flags_t oflags,
			struct udevice **devp)
{
	struct udevice *fil, *fsdev = dev_get_parent(dir);
	struct virtio_fs_dir_priv *dir_priv = dev_get_priv(dir);
	struct file_uc_priv *file_uc_priv;
	struct file_priv *file_priv;
	struct fuse_entry_out out;
	uint flags;
	u64 fh;
	int ret;

	log_debug("dir '%s' inode %llx leaf '%s' oflags %d\n", dir->name,
		  dir_priv->inode, leaf, oflags);

	ret = virtio_fs_lookup_(fsdev, dir_priv->inode, leaf, &out);
	if (ret) {
		log_debug("lookup fail ret=%d\n", ret);
		return log_msg_ret("vfl", ret);
	}

	log_debug("open nodeid %lld\n", out.nodeid);
	ret = virtio_fs_open_file(fsdev, out.nodeid, oflags, &fh, &flags);
	if (ret) {
		log_debug("fail ret=%d\n", ret);
		return log_msg_ret("vfo", ret);
	}
	log_debug("result fh %llx flags %x\n", fh, flags);

	ret = file_add_probe(dir, DM_DRIVER_REF(virtio_fs_file), leaf,
			     out.attr.size, flags, &fil);
	if (ret) {
		/* TODO: close file? */
		return log_msg_ret("vfp", ret);
	}

	file_priv = dev_get_priv(fil);
	file_priv->nodeid = out.nodeid;
	file_priv->fh = fh;
	file_priv->flags = flags;
	file_uc_priv = dev_get_uclass_priv(fil);

	log_debug("opened file dev '%s' inode %lld size %lx\n", fil->name,
		  file_priv->nodeid, file_uc_priv->size);
	*devp = fil;

	return 0;
}
