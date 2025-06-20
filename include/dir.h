/* SPDX-License-Identifier: GPL-2.0 */
/*
 * U-Boot Filesystem directory
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#ifndef __DIR_H
#define __DIR_H

struct fs_dirent;
struct fs_dir_stream;
struct udevice;

/**
 * struct dir_ops - Operations on directories
 */
struct dir_ops {
	/**
	 * open() - Open a directory for reading
	 *
	 * @dev: Directory device (UCLASS_DIR)
	 * @strmp: Returns an allocated pointer to the new stream
	 */
	int (*open)(struct udevice *dev, struct fs_dir_stream **strmp);

	/**
	 * read() - Read a single directory entry
	 *
	 * @dev: Directory device (UCLASS_DIR)
	 * @strm: Directory stream as created by open()
	 * @dentp: Return an allocated entry on success
	 * Return: 0 if OK, -ENOENT if no more entries, other -ve value on error
	 */
	int (*read)(struct udevice *dev, struct fs_dir_stream *strm,
		    struct fs_dirent **dentp);

	/**
	 * close() - Stop reading the directory
	 *
	 * Frees @strm and releases the directory
	 *
	 * @dev: Directory device (UCLASS_DIR)
	 * @strm: Directory stream as created by virtio_fs_compat_opendir()
	 * Return: 0 if OK, -ve on error
	 */
	int (*close)(struct udevice *dev, struct fs_dir_stream *strm);
};

/* Get access to a directory's operations */
#define dir_get_ops(dev)		((struct dir_ops *)(dev)->driver->ops)

/**
 * dir_open() - Open a directory for reading
 *
 * @dev: Directory device (UCLASS_DIR)
 * @strmp: Returns an allocated pointer to the new stream
 */
int dir_open(struct udevice *dev, struct fs_dir_stream **strmp);

/**
 * dir_read() - Read a single directory entry
 *
 * @dev: Directory device (UCLASS_DIR)
 * @strm: Directory stream as created by open()
 * @dentp: Return an allocated entry on success
 * Return: 0 if OK, -ENOENT if no more entries, other -ve value on other
 *	error
 */
int dir_read(struct udevice *dev, struct fs_dir_stream *strm,
	     struct fs_dirent **dentp);

/**
 * dir_close() - Stop reading the directory
 *
 * Frees @strm and releases the directory
 *
 * @dev: Directory device (UCLASS_DIR)
 * @strm: Directory stream as created by virtio_fs_compat_opendir()
 * Return: 0 if OK, -ve on error
 */
int dir_close(struct udevice *dev, struct fs_dir_stream *strm);

#endif
