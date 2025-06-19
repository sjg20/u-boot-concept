/* SPDX-License-Identifier: GPL-2.0 */

#ifndef VIRTIO_FS_H
#define VIRTIO_FS_H

struct blk_desc;
struct disk_partition;
struct fs_dirent;
struct fs_dir_stream;

int virtio_fs_opendir(const char *filename, struct fs_dir_stream **dirsp);
int virtio_fs_readdir(struct fs_dir_stream *dirs, struct fs_dirent **dentp);
void virtio_fs_closedir(struct fs_dir_stream *dirs);
int virtio_fs_probe(struct blk_desc *fs_dev_desc,
		    struct disk_partition *fs_partition);

#endif
