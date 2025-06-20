// SPDX-License-Identifier: GPL-2.0
/*
 * U-Boot Virtio-FS Driver
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 *
 * This driver provides U-Boot with the ability to access a virtio-fs
 * device, allowing the bootloader to read files from a shared directory
 * on the host. This is particularly useful for loading kernels, device
 * tree blobs, and other boot-time resources.
 *
 * The driver is implemented using the U-Boot driver model and the virtio
 * uclass. It communicates with the host using the FUSE protocol over
 * virtqueues.
 *
 * Licensed under the GPL-2.0+ license.
 */

#define LOG_DEBUG
#define LOG_CATEGORY	UCLASS_VIRTIO

#include <dir.h>
#include <dm.h>
#include <fs.h>
#include <log.h>
#include <malloc.h>
#include <virtio.h>
#include <virtio_fs.h>
#include <virtio_ring.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <linux/fuse.h>

#define VIRTIO_FS_TAG_SIZE	36

/**
 * struct virtio_fs_config - Configuration info for virtio-fs
 *
 * Modelled on Linux v6.15 include/uapi/linux/type.h
 *
 * @tag: filesystem name padded with NUL
 */
struct __packed virtio_fs_config {
	u8 tag[VIRTIO_FS_TAG_SIZE];
	__le32 unused1;
	__le32 unused2;
};

/*
 * Driver-private data
 *
 * @root: inode of the root node, or 0 if not known
 */
struct virtio_fs_priv {
	char tag[VIRTIO_FS_TAG_SIZE + 1];
	uint num_queues;
	uint notify_buf_size;
	struct virtio_device	*vdev;
	struct virtqueue	*vq;
	struct virtio_fs_config	config;
	u64 root_inode;
	int next_id;
};

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

static int virtio_fs_xfer(struct udevice *dev, struct fuse_in_header *inhdr,
			  const void *in, int insize,
			  struct fuse_out_header *outhdr, void *out,
			  int outsize)
{
	struct virtio_fs_priv *priv = dev_get_priv(dev);
	struct virtqueue *vq = priv->vq;
	struct virtio_sg sg[4];
	struct virtio_sg *sgs[4];
	uint dummy_len;
	int i, ret;

	inhdr->unique = priv->next_id++;

	sg[0].addr = inhdr;
	sg[0].length = sizeof(*inhdr);
	sg[1].addr = (void *)in;
	sg[1].length = insize;
	sg[2].addr = outhdr;
	sg[2].length = sizeof(*outhdr);
	sg[3].addr = out;
	sg[3].length = outsize;
	for (i = 0; i < 4; i++)
		sgs[i] = &sg[i];
	inhdr->len = sizeof(*inhdr) + insize;
	outhdr->len = sizeof(*outhdr) + outsize;

	log_debug("sg[1].addr %p sg[3].length %lx\n", sg[1].addr, sg[3].length);
	ret = virtqueue_add(vq, sgs, 2, out ? 2 : 1);
	if (ret) {
		log_err("Failed to add buffers to virtqueue\n");
		return ret;
	}

	virtqueue_kick(vq);

	log_debug("wait...");
	while (!virtqueue_get_buf(vq, &dummy_len))
		;
	log_debug("done\n");

	return 0;
}

static int virtio_fs_lookup(struct udevice *dev, u64 nodeid,
			    const char *name, struct fuse_entry_out *entry)
{
	struct fuse_in_header inhdr = {};
	struct fuse_out_header outhdr;
	int name_len = strlen(name) + 1;
	int ret;

	inhdr.opcode = FUSE_LOOKUP;
	inhdr.nodeid = nodeid;

	ret = virtio_fs_xfer(dev, &inhdr, name, name_len, &outhdr, entry,
			     sizeof(*entry));
	if (ret)
		return log_msg_ret("vfl", outhdr.error);
	log_debug("len %x error %x unique %llx\n", outhdr.len, outhdr.error,
		  outhdr.unique);
	if (outhdr.error)
		return log_msg_ret("vfL", outhdr.error);

	return 0;
}

static int virtio_fs_forget(struct udevice *dev, u64 nodeid)
{
	struct fuse_in_header inhdr = {};
	struct fuse_forget_in in;
	struct fuse_out_header outhdr;
	int ret;

	inhdr.opcode = FUSE_FORGET;
	inhdr.nodeid = nodeid;
	in.nlookup = 1;

	ret = virtio_fs_xfer(dev, &inhdr, &in, sizeof(in), &outhdr, NULL, 0);
	if (ret)
		return log_msg_ret("vfl", outhdr.error);
	log_debug("len %x error %x unique %llx\n", outhdr.len, outhdr.error,
		  outhdr.unique);
	if (outhdr.error)
		return log_msg_ret("vfL", outhdr.error);

	return 0;
}

static int _virtio_fs_opendir(struct udevice *dev, u64 nodeid,
			      struct fuse_open_out *out)
{
	struct fuse_in_header inhdr = {};
	struct fuse_open_in in = {};
	struct fuse_out_header outhdr;
	int ret;

	inhdr.opcode = FUSE_OPENDIR;
	inhdr.nodeid = nodeid;

	ret = virtio_fs_xfer(dev, &inhdr, &in, sizeof(in), &outhdr, out,
			     sizeof(*out));
	if (ret)
		return log_msg_ret("vfo", outhdr.error);
	log_debug("len %x error %x unique %llx\n", outhdr.len, outhdr.error,
		  outhdr.unique);
	if (outhdr.error)
		return log_msg_ret("vfO", outhdr.error);

	return 0;
}

static int _virtio_fs_readdir(struct udevice *dev, u64 nodeid, u64 fh,
			      u64 offset, void *buf, int size, int *out_sizep)
{
	struct fuse_in_header inhdr = {};
	struct fuse_read_in in = {};
	struct fuse_out_header outhdr;
	uint len;
	int ret;

	inhdr.opcode = FUSE_READDIRPLUS;
	inhdr.nodeid = nodeid;

	in.fh = fh;
	in.size = size;
	in.offset = offset;
	ret = virtio_fs_xfer(dev, &inhdr, &in, sizeof(in), &outhdr, buf, size);
	if (ret)
		return log_msg_ret("vfr", outhdr.error);
	log_debug("len %x error %x unique %llx\n", outhdr.len, outhdr.error,
		  outhdr.unique);
	if (outhdr.error)
		return log_msg_ret("vfR", outhdr.error);

	len = outhdr.len - sizeof(outhdr);
	/* sanity check */
	if (len > size) {
		log_debug("virtio: internal size error outhdr.len %x size %x\n",
			  outhdr.len, size);
		return log_msg_ret("vle", -EFAULT);
	}
	*out_sizep = len;

	return 0;
}

static int _virtio_fs_releasedir(struct udevice *dev, u64 nodeid, u64 fh)
{
	struct fuse_in_header inhdr = {};
	struct fuse_release_in in = {};
	struct fuse_out_header outhdr;
	int ret;

	inhdr.opcode = FUSE_RELEASEDIR;
	inhdr.nodeid = nodeid;
	in.fh = fh;

	ret = virtio_fs_xfer(dev, &inhdr, &in, sizeof(in), &outhdr, NULL, 0);
	if (ret)
		return log_msg_ret("vfe", outhdr.error);
	log_debug("len %x error %x unique %llx\n", outhdr.len, outhdr.error,
		  outhdr.unique);
	if (outhdr.error)
		return log_msg_ret("vfE", outhdr.error);

	return 0;
}

static int virtio_fs_init(struct udevice *dev)
{
	struct fuse_in_header inhdr = {};
	struct fuse_init_in in = {};
	struct fuse_out_header outhdr;
	struct fuse_init_out out;
	int ret;

	inhdr.opcode = FUSE_INIT;

	in.major = FUSE_KERNEL_VERSION;
	in.minor = FUSE_KERNEL_MINOR_VERSION;

	ret = virtio_fs_xfer(dev, &inhdr, &in, sizeof(in), &outhdr, &out,
			     sizeof(out));
	if (ret)
		return log_msg_ret("vfx", ret);
	log_debug("major %x minor %x max_readahead %x flags %x ", out.major,
		  out.minor, out.max_readahead, out.flags);
	log_debug("max_background %x congestion_threshold %x max_write %x\n",
		  out.max_background, out.congestion_threshold, out.max_write);
	log_debug("time_gran %x max_pages %x, map_alignment %x flags2 %x ",
		  out.time_gran, out.max_pages, out.map_alignment, out.flags2);
	log_debug("max_stack_depth %x request_timeout %x\n",
		  out.max_stack_depth, out.request_timeout);

	return 0;
}

static int _virtio_fs_probe(struct udevice *dev)
{
	struct virtio_fs_priv *priv = dev_get_priv(dev);
	int ret;

	virtio_cread_bytes(dev, 0, &priv->tag, VIRTIO_FS_TAG_SIZE);
	priv->tag[VIRTIO_FS_TAG_SIZE] = '\0';

	log_debug("tag %s\n", priv->tag);

	ret = virtio_find_vqs(dev, 1, &priv->vq);
	if (ret)
		return log_msg_ret("vff", ret);

	return 0;
}

int virtio_fs_ls(struct udevice *dev, const char *path)
{
	struct fuse_entry_out entry;
	struct fuse_open_out out;
	struct fuse_direntplus *ent;
	char buf[0x400];
	int ret, size;
	u64 rinode, fh, pinode;
	u64 offset;

	ret = virtio_fs_init(dev);
	if (ret)
		return log_msg_ret("vfi", ret);

	rinode = FUSE_ROOT_ID;
	ret = virtio_fs_lookup(dev, FUSE_ROOT_ID, ".", &entry);
	if (ret) {
		log_err("1Failed to lookup root directory: %d\n", ret);
		return ret;
	}

	log_debug("directory found, ino=%lld %lld\n", rinode, entry.attr.ino);

	if (path) {
		rinode = entry.nodeid;
		log_debug("looking up path '%s' (inode %lld)\n", path, rinode);
		ret = virtio_fs_lookup(dev, rinode, path, &entry);
		if (ret) {
			log_err("2Failed to lookup directory '%s': %d\n", path,
				ret);
			return ret;
		}
		pinode = entry.nodeid;
	} else {
		pinode = rinode;
	}

	ret = _virtio_fs_opendir(dev, pinode, &out);
	if (ret) {
		log_err("Failed to open root directory: %d\n", ret);
		return ret;
	}
	fh = out.fh;
	log_debug("fh %llx open_flags %x backing_id %x\n", fh, out.open_flags,
		  out.backing_id);

	offset = 0;
	printf("%10s  Type  Name\n", "Size");
	ret = 0;
	do {
		ret = _virtio_fs_readdir(dev, pinode, fh, offset, buf,
					 sizeof(buf), &size);
		if (ret) {
			log_err("Failed to read directory: %d\n", ret);
			break;
		}

		if (!size)
			break;

		log_debug("virtio-fs: size %x\n", size);

		ent = (struct fuse_direntplus *)buf;
		while (size >= FUSE_NAME_OFFSET) {
			size_t reclen = FUSE_DIRENTPLUS_SIZE(ent);
			struct fuse_attr *attr = &ent->entry_out.attr;

			printf("%10llx  %4d  %.*s%s\n", attr->size,
			       ent->dirent.type, ent->dirent.namelen,
			       ent->dirent.name,
			       ent->dirent.type == FS_DT_DIR ? "/" :
			       ent->dirent.type == FS_DT_LNK ? " >" : "");
			offset = ent->dirent.off;
			ent = (void *)ent + reclen;
			size -= reclen;
		}
	} while (1);

	log_debug("releasedir\n");
	ret = _virtio_fs_releasedir(dev, pinode, fh);
	if (ret) {
		log_err("Failed to release directory: %d\n", ret);
		return ret;
	}

	log_debug("forget\n");
	ret = virtio_fs_forget(dev, pinode);
	if (ret)
		return log_msg_ret("pfo", ret);
	if (path) {
		ret = virtio_fs_forget(dev, rinode);
		if (ret)
			return log_msg_ret("rfo", ret);
	}

	return 0;
}

static int virtio_fs_dir_open(struct udevice *dev, struct fs_dir_stream **dirsp)
{
	struct virtio_fs_dir_priv *dir_priv = dev_get_priv(dev);
	struct udevice *fs = dev_get_parent(dev);
	struct fs_dir_stream *strm;
	struct fuse_open_out out;
	int ret;

	strm = malloc(sizeof(struct fs_dir_stream));
	if (!strm)
		return log_msg_ret("vso", -ENOMEM);

	ret = _virtio_fs_opendir(fs, dir_priv->inode, &out);
	if (ret) {
		log_err("Failed to open root directory: %d\n", ret);
		return ret;
	}
	strm->dev = dev;
	strm->fh = out.fh;
	log_debug("fh %llx open_flags %x backing_id %x\n", strm->fh,
		  out.open_flags, out.backing_id);

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
	char buf[0x100];
	int ret, size;

	log_debug("virtio_fs_dir_read %lld strm %p\n", dir_priv->inode, strm);
	log_debug("offset %lld\n", strm->offset);
	ret = _virtio_fs_readdir(fs, dir_priv->inode, strm->fh, strm->offset,
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

	printf("%10llx  %4d  %.*s%s\n", attr->size,
		       ent->dirent.type, ent->dirent.namelen,
		       ent->dirent.name,
		       ent->dirent.type == FS_DT_DIR ? "/" :
		       ent->dirent.type == FS_DT_LNK ? " >" : "");
	strm->offset = ent->dirent.off;

	rec = calloc(1, sizeof(struct fs_dirent));
	if (!rec)
		return log_msg_ret("vde", -ENOMEM);
	rec->type = ent->dirent.type;
	rec->size = attr->size;
	rec->attr = attr->flags;
	strlcpy(rec->name, ent->dirent.name,
		min((int)ent->dirent.namelen, FS_DIRENT_NAME_LEN));
	*dentp = rec;

	return 0;
}

static int virtio_fs_dir_remove(struct udevice *dev)
{
	struct virtio_fs_dir_priv *dir_priv = dev_get_priv(dev);

	if (dir_priv->path) {
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

static int virtio_fs_lookup_dir(struct udevice *dev, const char *path,
				struct udevice **dirp)
{
	struct virtio_fs_priv *priv = dev_get_priv(dev);
	struct virtio_fs_dir_priv *dir_priv;
	struct fuse_entry_out entry;
	char dev_name[30], *str, *dup_path;
	struct udevice *dir;
	u64 inode;
	int ret;

	inode = priv->root_inode;
	if (path && strcmp("/", path)) {
		log_debug("looking up path '%s' (inode %lld)\n", path, inode);
		ret = virtio_fs_lookup(dev, inode, path, &entry);
		if (ret) {
			log_err("Failed to lookup directory '%s': %d\n", path,
				ret);
			return ret;
		}
		inode = entry.nodeid;
	}

	snprintf(dev_name, sizeof(dev_name), "%s.dir", dev->name);
	ret = -ENOMEM;
	str = strdup(dev_name);
	if (!str)
		goto no_dev_name;
	dup_path = strdup(path);
	if (!str)
		goto no_dev_path;

	ret = device_bind_driver(dev, "virtio_fs_dir", str, &dir);
	if (ret)
		goto no_bind;
	device_set_name_alloced(dir);

	ret = device_probe(dir);
	if (ret)
		goto no_probe;

	dir_priv = dev_get_priv(dir);
	dir_priv->inode = inode;
	dir_priv->path = dup_path;

	*dirp = dir;

	return 0;

no_probe:
	device_unbind(dir);
no_bind:
	free(dup_path);
no_dev_path:
	free(str);
no_dev_name:
	if (path)
		ret = virtio_fs_forget(dev, inode);

	return ret;
}

static int virtio_fs_mount(struct udevice *dev)
{
	struct fs_priv *uc_priv = dev_get_uclass_priv(dev);
	struct virtio_fs_priv *priv = dev_get_priv(dev);
	struct fuse_entry_out entry;
	int ret;

	if (uc_priv->mounted)
		return log_msg_ret("vfi", -EISCONN);

	ret = virtio_fs_init(dev);
	if (ret)
		return log_msg_ret("vfi", ret);

	ret = virtio_fs_lookup(dev, FUSE_ROOT_ID, ".", &entry);
	if (ret) {
		log_err("Failed to lookup root directory: %d\n", ret);
		return ret;
	}
	log_debug("directory found, ino=%lld\n", entry.nodeid);

	priv->root_inode = entry.nodeid;
	uc_priv->mounted = true;

	return 0;
}

static int virtio_fs_unmount(struct udevice *dev)
{
	struct fs_priv *uc_priv = dev_get_uclass_priv(dev);
	struct virtio_fs_priv *priv = dev_get_priv(dev);
	int ret;

	if (!uc_priv->mounted)
		return log_msg_ret("vfu", -ENOTCONN);

	ret = virtio_fs_forget(dev, priv->root_inode);
	if (ret)
		return log_msg_ret("vff", ret);

	return 0;
}

static int virtio_fs_remove(struct udevice *dev)
{
	return 0;
}

static int virtio_fs_bind(struct udevice *dev)
{
	struct virtio_dev_priv *uc_priv = dev_get_uclass_priv(dev->parent);
	struct fs_plat *uc_plat = dev_get_uclass_priv(dev);

	static_assert(VIRTIO_FS_TAG_SIZE < FS_MAX_NAME_LEN);
	uc_plat->name[VIRTIO_FS_TAG_SIZE] = '\0';

	/* Indicate what driver features we support */
	virtio_driver_features_init(uc_priv, NULL, 0, NULL, 0);

	return 0;
}

static const struct fs_ops virtio_fs_ops = {
	.mount		= virtio_fs_mount,
	.unmount	= virtio_fs_unmount,
	.lookup_dir	= virtio_fs_lookup_dir,
};

static const struct udevice_id virtio_fs_ids[] = {
	{ .compatible = "virtio,fs" },
	{ }
};

U_BOOT_DRIVER(virtio_fs) = {
	.name	= VIRTIO_FS_DRV_NAME,
	.id	= UCLASS_FS,
	.of_match = virtio_fs_ids,
	.ops	= &virtio_fs_ops,
	.bind	= virtio_fs_bind,
	.probe	= _virtio_fs_probe,
	.remove	= virtio_fs_remove,
	.priv_auto	= sizeof(struct virtio_fs_priv),
	.flags	= DM_FLAG_ACTIVE_DMA,
};
