// SPDX-License-Identifier: GPL-2.0
/*
 * U-Boot Virtio-FS directories
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 *
 * Supports access to directories in virtio-fs
 */

#ifndef _FS_INTERNAL_H
#define _FS_INTERNAL_H

#include <dir.h>

struct fuse_entry_out;
struct udevice;

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

/**
 * virtio_fs_lookup() - Look up an entry in a directory
 *
 * @dev: Filesystem device which holds the directory (UCLASS_FS)
 * @nodeid: Node ID of the directory containing the entry
 * @name: Name of the entry
 * @out: Returns lookup info
 * Return: 0 on success, -ENOENT if not found, other -ve on other error
 */
int virtio_fs_lookup_(struct udevice *dev, u64 nodeid, const char *name,
		      struct fuse_entry_out *out);

/**
 * virtio_fs_lookup() - Look up an entry in a directory
 *
 * This is a simplified wrapper around virtio_fs_lookup_() that performs a
 * lookup within the filesystem's root directory.
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
 * virtio_fs_opendir() - Open a directory for reading
 *
 * @dev: Filesystem device which holds the directory (UCLASS_FS)
 * @nodeid: Node ID of the directory
 * @fhp: Returns unique filehandle for the directory
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
 * virtio_fs_open_file() - Open a file
 *
 * @dev: Filesystem device which holds the file (UCLASS_FS)
 * @nodeid: Node ID of the directory containing the file
 * @flags: Open-mode flags to use
 * @fhp: Return unique filehandle for the directory
 * @flagsp: Updated open-mode flags
 */
int virtio_fs_open_file(struct udevice *dev, u64 nodeid,
			enum dir_open_flags_t flags, u64 *fhp, uint *flagsp);

/**
 * virtio_fs_get_root() - Get the nodeid of the root directory of the virtio-fs
 *
 * Return: node ID of root node
 */
u64 virtio_fs_get_root(struct udevice *dev);

/**
 * virtio_fs_read() - Read data from an open file
 *
 * Read a block of data from a file previously opened with
 * virtio_fs_open_file()
 *
 * @dev: Filesystem device (UCLASS_FS)
 * @nodeid: Node ID of the file being read (for context)
 * @fh: File handle of the open file
 * @offset: Offset within the file to start reading from
 * @buf: Buffer to store the read data
 * @size: Maximum number of bytes to read
 * Return: Number of bytes read on success, -ve on error
 */
long virtio_fs_read(struct udevice *dev, u64 nodeid, u64 fh, u64 offset,
		    void *buf, uint size);

/**
 * virtio_fs_setup_dir() - Look up a directory and create a device for it
 *
 * Looks up a path to find the corresponding inode in the virtio-fs filesystem,
 * then creates and probes a new 'directory' device to represent it.
 *
 * If the path is the root (NULL or "/"), it uses the root inode directly.
 *
 * @fsdev: The virtio-fs filesystem device
 * @path: The path of the directory to set up (e.g., "/boot" or "/")
 * @devp: On success, returns a pointer to the newly created directory device
 * Return: 0 on success, -ve on error
 */
int virtio_fs_setup_dir(struct udevice *fsdev, const char *path,
			struct udevice **devp);

/**
 * virtio_fs_setup_file() - Look up and open a file, creating a new device
 *
 * Sets up a new open file: performs a lookup for the file within a given
 * directory, opens it via FUSE, then probes and adds a new 'file' device to
 * represent the opened file
 *
 * @dir: The directory device in which to look for the file
 * @leaf: The name of the file to open (the leaf name)
 * @oflags: Open flags to use when opening the file
 * @devp: On success, returns a pointer to the newly created file device
 * Return: 0 on success, -ve on error
 */
int virtio_fs_setup_file(struct udevice *dir, const char *leaf,
			enum dir_open_flags_t flags,
			struct udevice **devp);

#endif
