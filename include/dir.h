/* SPDX-License-Identifier: GPL-2.0 */
/*
 * U-Boot Filesystem directory
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#ifndef __DIR_H
#define __DIR_H

struct driver;
struct fs_dirent;
struct fs_dir_stream;
struct udevice;

/**
 * enum dir_open_flags_t - Flags to control the open mode of files
 *
 * @DIR_O_RDONLY: Open the file read-only
 * @DIR_O_WRONLY: Open the file write-only, overwriting existing file contents
 * @DIR_O_RDWR: Open the file for read/write, allowing the file to be updated
 */
enum dir_open_flags_t {
	DIR_O_RDONLY,
	DIR_O_WRONLY,
	DIR_O_RDWR,
};

/**
 * struct dir_uc_priv - Uclass information for each directory
 *
 * @path: Absolute path to directory, "" for root
 */
struct dir_uc_priv {
	char *path;
};

/**
 * struct dir_ops - Operations on directories
 */
struct dir_ops {
	/**
	 * open() - Open a directory for reading
	 *
	 * @dev: Directory device (UCLASS_DIR)
	 * @strm: Stream information to fill in, on success (zeroed on entry)
	 */
	int (*open)(struct udevice *dev, struct fs_dir_stream *strm);

	/**
	 * read() - Read a single directory entry
	 *
	 * @dev: Directory device (UCLASS_DIR)
	 * @strm: Directory stream as created by open()
	 * @dent: Directory entry to fill in (zeroed on entry)
	 * Return: 0 if OK, -ENOENT if no more entries, other -ve value on error
	 */
	int (*read)(struct udevice *dev, struct fs_dir_stream *strm,
		    struct fs_dirent *dent);

	/**
	 * close() - Stop reading the directory
	 *
	 * @dev: Directory device (UCLASS_DIR)
	 * @strm: Directory stream as created by dir_opendir()
	 * Return: 0 if OK, -ve on error
	 */
	int (*close)(struct udevice *dev, struct fs_dir_stream *strm);

	/**
	 * open_file() - Create a new file device for a file
	 *
	 * @dev: Directory device (UCLASS_DIR)
	 * @leaf: Filename within the directory
	 * @flags: Open-mode flags to use
	 * @filp: Returns the UCLASS_FILE device
	 * Return: 0 if OK, -ve on error
	 */
	int (*open_file)(struct udevice *dev, const char *leaf,
			 enum dir_open_flags_t oflags, struct udevice **filp);
};

/* Get access to a directory's operations */
#define dir_get_ops(dev)		((struct dir_ops *)(dev)->driver->ops)

/**
 * dir_open() - Open a directory for reading
 *
 * @dev: Directory device (UCLASS_DIR)
 * @strm: Returns allocated pointer to stream information, on success
 */
int dir_open(struct udevice *dev, struct fs_dir_stream **strmp);

/**
 * dir_read() - Read a single directory entry
 *
 * @dev: Directory device (UCLASS_DIR)
 * @strm: Directory stream as created by open()
 * @dent: Directory entry to fill in
 * Return: 0 if OK, -ENOENT if no more entries, other -ve value on other
 *	error
 */
int dir_read(struct udevice *dev, struct fs_dir_stream *strm,
	     struct fs_dirent *dent);

/**
 * dir_close() - Stop reading the directory
 *
 * Frees @strm and releases the directory
 *
 * @dev: Directory device (UCLASS_DIR)
 * @strm: Directory stream as created by dir_opendir()
 * Return: 0 if OK, -ve on error
 */
int dir_close(struct udevice *dev, struct fs_dir_stream *strm);

/**
 * dir_add_probe() - Add a new directory and probe it
 *
 * @fsdev: Filesystem containing the directory
 * @drv: Driver to use
 * @path Absolute path to directory (within the filesystem), or NULL/"/" for
 *	root
 * @devp: Returns the new device, probed ready for use *
 */
int dir_add_probe(struct udevice *fsdev, struct driver *drv, const char *path,
		  struct udevice **devp);

/**
 * dir_open_file() - Create a new file device for a file
 *
 * @dev: Directory device (UCLASS_DIR)
 * @leaf: Filename within the directory
 * @flags: Open-mode flags to use
 * @filp: Returns the UCLASS_FILE device
 * Return: 0 if OK, -ve on error
 */
int dir_open_file(struct udevice *dev, const char *leaf,
		  enum dir_open_flags_t oflags, struct udevice **filp);

#endif
