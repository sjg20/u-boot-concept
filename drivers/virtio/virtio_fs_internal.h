// SPDX-License-Identifier: GPL-2.0
/*
 * U-Boot Virtio-FS directories
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 *
 * Supports access to directories in virtio-fs
 */

#ifndef _VIRTIO_FS_INTERNAL_H
#define _VIRTIO_FS_INTERNAL_H

#define LOG_CATEGORY	UCLASS_VIRTIO

struct udevice;

/**
 * virtio_fs_opendir() - Open a directory for reading
 *
 * @dev: Filesystem device which holds the directory (UCLASS_FS)
 * @nodeid: Node ID of the directory
 * @fhp: Return unique filehandle for the directory
 * Return: 0 if OK, -ve on error
 */
int virtio_fs_opendir(struct udevice *dev, u64 nodeid, u64 *fhp);

/**
 * virtio_fs_readdir() - Read a chunk of entries from a directory
 *
 * Fills in @buf with directory records, using an internal FUSE format. The
 * format is one struct fuse_direntplus (plus a name string) for each record.
 * See include/linux/fuse.h for the struct details (unfortunately not
 * commented). Use FUSE_DIRENTPLUS_SIZE() to calculate the size of each entry.
 *
 * It would be possible to convert the format into struct fs_dirent but the
 * API which uses it only allows returning a single record at a time, so it
 * seems best to tackle that later.
 *
 * @dev: Filesystem device which holds the directory (UCLASS_FS)
 * @nodeid: Node ID of the directory
 * @fh: Unique filehandle for the directory
 * @offset: Start offset for reading; use the value returned from the previous
 *	call, or 0 to start at the beginning
 * @buf: Buffer to receive records, must be large enough to hold at least one
 *	struct fuse_direntplus
 * @size: Size of buffer
 * @out_sizep: Returns amount of data used in the buffer
 */
int virtio_fs_readdir(struct udevice *dev, u64 nodeid, u64 fh, u64 offset,
		      void *buf, int size, int *out_sizep);

/**
 * virtio_fs_releasedir() - Close a directory
 *
 * Use this on a directory opened with virtio_fs_opendir() when you have
 * finished reading entries with virtio_fs_readdir()
 *
 * @dev: Filesystem device which holds the directory (UCLASS_FS)
 * @nodeid: Node ID of the directory
 * @fh: Unique filehandle for the directory
 */
int virtio_fs_releasedir(struct udevice *dev, u64 nodeid, u64 fh);

/**
 * virtio_fs_lookup() - Look up an entry in a directory
 *
 * For now this only supports the root directory
 *
 * @dev: Filesystem device which holds the directory (UCLASS_FS)
 * @name: Name of the entry
 * @entryp: Returns the node ID of the entry, on success
 * Return: 0 on success, -ENOENT if not found, other -ve on other error
 */
int virtio_fs_lookup(struct udevice *dev, const char *name, u64 *entryp);

/**
 * virtio_fs_forget() - Forget a nodeid
 *
 * Tells FUSE that this nodeid is no-longer needed
 *
 * @dev: Filesystem device which holds the directory (UCLASS_FS)
 * @nodeid: Node ID to forget
 * Return: 0 on success, -ve on error
 */
int virtio_fs_forget(struct udevice *dev, u64 nodeid);

/**
 * virtio_fs_lookup_dir() -
 */
int virtio_fs_lookup_dir(struct udevice *fsdev, const char *path,
			 struct udevice **devp);

#endif
