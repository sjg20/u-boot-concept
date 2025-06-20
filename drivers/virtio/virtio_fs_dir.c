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
#include "virtio_fs_internal.h"

/**
 * struct virtio_fs_dir_priv - Information about a directory
 *
 * @inode: Associated inode for the directory
 * @path: Path of this directory, e.g. '/fred/mary', or NULL for the root
 * directory
 */
struct virtio_fs_dir_priv {
	u64 inode;
	char *path;
};

static int virtio_fs_dir_open(struct udevice *dev, struct fs_dir_stream **dirsp)
{
	struct virtio_fs_dir_priv *dir_priv = dev_get_priv(dev);
	struct udevice *fs = dev_get_parent(dev);
	struct fs_dir_stream *strm;
	int ret;

	strm = malloc(sizeof(struct fs_dir_stream));
	if (!strm)
		return log_msg_ret("vso", -ENOMEM);

	log_debug("opening inode %lld\n", dir_priv->inode);
	ret = virtio_fs_opendir(fs, dir_priv->inode, &strm->fh);
	if (ret) {
		log_err("Failed to open directory: %d\n", ret);
		return ret;
	}
	strm->dev = dev;

	strm->offset = 0;
	*dirsp = strm;

	return 0;
}

int virtio_fs_dir_read(struct udevice *dev, struct fs_dir_stream *strm,
		       struct fs_dirent **dentp)
{
	struct virtio_fs_dir_priv *dir_priv = dev_get_priv(dev);
	struct udevice *fs = dev_get_parent(dev);
	struct fuse_direntplus *ent;
	struct fs_dirent *rec;
	size_t reclen;
	struct fuse_attr *attr;
	char buf[0x200];
	int ret, size;

	log_debug("virtio_fs_dir_read %lld strm %p\n", dir_priv->inode, strm);
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

	rec = calloc(1, sizeof(struct fs_dirent));
	if (!rec)
		return log_msg_ret("vde", -ENOMEM);
	rec->type = ent->dirent.type;
	rec->size = attr->size;
	rec->attr = attr->flags;
	strlcpy(rec->name, ent->dirent.name,
		min((int)ent->dirent.namelen + 1, FS_DIRENT_NAME_LEN));
	*dentp = rec;

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

	log_debug("free\n");
	free(strm);
	log_debug("close done\n");

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

int virtio_fs_lookup_dir(struct udevice *fsdev, const char *path,
			 struct udevice **devp)
{
	struct virtio_fs_dir_priv *dir_priv;
	char dev_name[30], *str, *dup_path;
	struct udevice *dir;
	bool has_path;
	u64 inode;
	int ret;

	inode = FUSE_ROOT_ID;
	has_path = path && strcmp("/", path);
	if (has_path) {
		log_debug("looking up path '%s'\n", path);
		ret = virtio_fs_lookup(fsdev, path, &inode);
		if (ret) {
			log_err("Failed to lookup directory '%s': %d\n", path,
				ret);
			return ret;
		}
		log_debug("got inode %lld\n", inode);
	}

	snprintf(dev_name, sizeof(dev_name), "%s.dir", fsdev->name);
	ret = -ENOMEM;
	str = strdup(dev_name);
	if (!str)
		goto no_dev_name;
	dup_path = strdup(has_path ? path : "");
	if (!str)
		goto no_dev_path;

	ret = device_bind_driver(fsdev, "virtio_fs_dir", str, &dir);
	if (ret)
		goto no_bind;
	device_set_name_alloced(dir);

	ret = device_probe(dir);
	if (ret)
		goto no_probe;

	dir_priv = dev_get_priv(dir);
	dir_priv->inode = inode;
	dir_priv->path = dup_path;

	*devp = dir;

	return 0;

no_probe:
	device_unbind(dir);
no_bind:
	free(dup_path);
no_dev_path:
	free(str);
no_dev_name:
	if (path)
		ret = virtio_fs_forget(fsdev, inode);

	return ret;
}
