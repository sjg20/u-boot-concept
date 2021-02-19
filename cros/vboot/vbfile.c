// SPDX-License-Identifier: GPL-2.0+
/*
 * Interface for accessing files in SPI flash
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <cbfs.h>
#include <log.h>
#include <cros/cros_ofnode.h>
#include <cros/fwstore.h>
#include <cros/vbfile.h>
#include <cros/vboot.h>

int vbfile_load(struct vboot_info *vboot, const char *name,
		u8 **imagep, int *image_sizep)
{
	int ret;

	if (!vboot->from_coreboot) {
		struct fmap_entry entry;

		ret = cros_ofnode_find_locale("locales", &entry);
		if (ret)
			return log_msg_ret("find", ret);

		/* Load locale list */
		ret = fwstore_load_image(vboot->fwstore, &entry, imagep,
					 image_sizep);
		if (ret)
			return log_msg_ret("read", ret);
	} else {
		const struct cbfs_cachenode *file;

		file = cbfs_find_file(vboot->cbfs, name);
		if (!file)
			return log_msg_ret("cfind", -ENOENT);
		printf("file %s, data %p, attr %x\n", file->name, file->data,
		       file->attr_offset);
		return -ENOENT;
	}

	return 0;
}
