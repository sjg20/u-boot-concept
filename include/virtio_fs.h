/* SPDX-License-Identifier: GPL-2.0 */

#ifndef VIRTIO_FS_H
#define VIRTIO_FS_H

struct blk_desc;
struct disk_partition;
struct fs_dirent;
struct fs_dir_stream;

/**
 * virtio_fs_compat_opendir() - Open a directory
 *
 * This is a compatibility interface for struct fstype_info
 *
 * @fname: Path to directory to open ("/" for root)
 * @strmp: Returns an allocated pointer to the new stream
 * Return 0 if OK, -ve on error
 */
int virtio_fs_compat_opendir(const char *fname, struct fs_dir_stream **strmp);

/**
 * virtio_fs_compat_readdir() - Read a single directory entry
 *
 * This is a compatibility interface for struct fstype_info
 *
 * @strm: Directory stream as created by virtio_fs_compat_opendir()
 * @dentp: Return an allocated entry on success
 * Return: 0 if OK, -ENOENT if no more entries, other -ve value on other error
 */
int virtio_fs_compat_readdir(struct fs_dir_stream *strm,
			     struct fs_dirent **dentp);

/**
 * virtio_fs_compat_closedir() - Stop reading the directory
 *
 * This is a compatibility interface for struct fstype_info
 *
 * Frees @strm and releases the directory
 *
 * @strm: Directory stream as created by virtio_fs_compat_opendir()
 */
void virtio_fs_compat_closedir(struct fs_dir_stream *strm);

/**
 * virtio_fs_compat_probe() - Probe for a virtio-fs filesystem
 *
 * This is a compatibility interface for struct fstype_info
 *
 * For now, this just locates the first available UCLASS_FS device and sets a
 * global variable to it, for use by the above virtio_fs_compat_...() functions.
 *
 * @fs_dev_desc: Block device (not used, can be NULL)
 * @fs_partition: Partition (not used, can be NULL)
 * Return: 0 if OK, -ve on error
 */
int virtio_fs_compat_probe(struct blk_desc *fs_dev_desc,
			   struct disk_partition *fs_partition);

/**
 * virtio_fs_compat_size() - Get the size of a file
 *
 * @fname: Filename to check
 * @sizep: Returns size of the file, on success
 * Return: 0 if OK, -ve on error
 */
int virtio_fs_compat_size(const char *fname, loff_t *sizep);

/**
 * virtio_fs_compat_read() - Read from a file
 *
 * @fname: Filename to read from
 * @buf: Buffer to read into
 * @offset: Offset within the file to start reading
 * @len: Number of bytes to read, or 0 to read all
 * @actread: Returns the number of bytes actually read, on success
 * Return: 0 if OK, -ve on error
 */
int virtio_fs_compat_read(const char *fname, void *buf, loff_t offset,
			  loff_t len, loff_t *actread);

#endif
