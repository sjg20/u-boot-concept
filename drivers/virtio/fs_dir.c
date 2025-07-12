// SPDX-License-Identifier: GPL-2.0
/*
 * U-Boot Virtio-FS directories
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 *
 * Supports access to directories in virtio-fs
 */

#define LOG_CATEGORY	UCLASS_VIRTIO

#include <dm.h>
#include <dir.h>
#include <fs.h>
#include <malloc.h>
#include <virtio_fs.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <linux/fuse.h>
#include "fs_internal.h"

static int virtio_fs_dir_open(struct udevice *dev, struct fs_dir_stream *strm)
{
	struct virtio_fs_dir_priv *dir_priv = dev_get_priv(dev);
	struct udevice *fs = dev_get_parent(dev);
	int ret;

	log_debug("opening inode %lld\n", dir_priv->inode);
	ret = virtio_fs_opendir(fs, dir_priv->inode, &strm->fh);
	log_debug("2 open ret %d strm %p fh %llx\n", ret, strm, strm->fh);
	if (ret) {
		log_err("Failed to open directory: %d\n", ret);
		return ret;
	}
	strm->dev = dev;
	strm->offset = 0;

	return 0;
}

int virtio_fs_dir_read(struct udevice *dev, struct fs_dir_stream *strm,
		       struct fs_dirent *dent)
{
	struct virtio_fs_dir_priv *dir_priv = dev_get_priv(dev);
	struct udevice *fs = dev_get_parent(dev);
	struct fuse_direntplus *ent;
	size_t reclen;
	struct fuse_attr *attr;
	char buf[0x200];
	int ret, size;

	log_debug("start %lld strm %p fh %llx\n", dir_priv->inode, strm,
		  strm->fh);
	log_debug("offset %lld\n", strm->offset);
	ret = virtio_fs_readdir(fs, dir_priv->inode, strm->fh, strm->offset,
				buf, sizeof(buf), &size);
	if (ret) {
		log_err("Failed to read directory: %d\n", ret);
		return ret;
	}

	if (!size)
		return log_msg_ret("vde", -ENOENT);

	log_debug("virtio-fs: size %x\n", size);

	ent = (struct fuse_direntplus *)buf;

	/* this shouldn't happen, but just to be sure... */
	if (size < FUSE_NAME_OFFSET)
		return -ENOSPC;

	reclen = FUSE_DIRENTPLUS_SIZE(ent);
	attr = &ent->entry_out.attr;
	strm->offset = ent->dirent.off;

	dent->type = ent->dirent.type;
	dent->size = attr->size;
	dent->attr = attr->flags;
	strlcpy(dent->name, ent->dirent.name,
		min((int)ent->dirent.namelen + 1, FS_DIRENT_NAME_LEN));

	return 0;
}

static int virtio_fs_dir_close(struct udevice *dev, struct fs_dir_stream *strm)
{
	struct virtio_fs_dir_priv *dir_priv = dev_get_priv(dev);
	struct udevice *fs = dev_get_parent(dev);
	int ret;

	log_debug("close\n");
	ret = virtio_fs_releasedir(fs, dir_priv->inode, strm->fh);
	log_debug("ret %d\n", ret);
	if (ret) {
		log_err("Failed to release directory: %d\n", ret);
		return ret;
	}

	log_debug("close done\n");

	return 0;
}

static int open_file(struct udevice *dev, const char *leaf,
		     enum dir_open_flags_t oflags, struct udevice **filp)
{
	struct udevice *fil;
	int ret;

	log_debug("start '%s'\n", leaf);
	ret = virtio_fs_setup_file(dev, leaf, oflags, &fil);
	log_debug("ret %d\n", ret);
	if (ret)
		return log_msg_ret("dof", ret);
	*filp = fil;

	return 0;
}

static int virtio_fs_dir_remove(struct udevice *dev)
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

static struct dir_ops virtio_fs_dir_ops = {
	.open	= virtio_fs_dir_open,
	.read	= virtio_fs_dir_read,
	.close	= virtio_fs_dir_close,
	.open_file	= open_file,
};

static const struct udevice_id dir_ids[] = {
	{ .compatible = "virtio-fs,directory" },
	{ }
};

U_BOOT_DRIVER(virtio_fs_dir) = {
	.name	= "virtio_fs_dir",
	.id	= UCLASS_DIR,
	.of_match = dir_ids,
	.remove	= virtio_fs_dir_remove,
	.ops	= &virtio_fs_dir_ops,
	.priv_auto	= sizeof(struct virtio_fs_dir_priv),
	.flags	= DM_FLAG_ACTIVE_DMA,
};

int virtio_fs_setup_dir(struct udevice *fsdev, const char *path,
			struct udevice **devp)
{
	struct virtio_fs_dir_priv *dir_priv;
	struct udevice *dir;
	u64 inode;
	int ret;

	log_debug("looking up path '%s'\n", path);
	inode = FUSE_ROOT_ID;
	if (*path) {
		ret = virtio_fs_lookup(fsdev, path, &inode);
		if (ret) {
			log_err("Failed to lookup directory '%s': %d\n", path,
				ret);
			return ret;
		}
		log_debug("got inode %lld\n", inode);
	}

	ret = dir_add_probe(fsdev, DM_DRIVER_REF(virtio_fs_dir), path, &dir);
	if (ret)
		goto no_add;

	dir_priv = dev_get_priv(dir);
	dir_priv->inode = inode;
	log_debug("added new dir '%s' inode %llx\n", path, inode);

	*devp = dir;

	return 0;

no_add:
	if (*path)
		ret = virtio_fs_forget(fsdev, inode);

	return ret;
}
