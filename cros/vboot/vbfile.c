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
#include <lzma/LzmaTypes.h>
#include <lzma/LzmaDec.h>
#include <lzma/LzmaTools.h>

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
		switch (file->comp_algo) {
		case CBFS_COMPRESS_NONE:
			*imagep = file->data;
			*image_sizep = file->data_length;
			break;
		case CBFS_COMPRESS_LZMA:{
			SizeT inout_size;
			void *buf;

			buf = malloc(file->decomp_size);
			if (!buf)
				return log_msg_ret("lzma", -ENOMEM);
			inout_size = file->decomp_size;
			ret = lzmaBuffToBuffDecompress(buf, &inout_size,
						       file->data,
						       file->data_length);
			if (ret) {
				log_warning("LZMA decomp failed, err=%d\n",
					    ret);
				return log_msg_ret("lzmad", -EIO);
			}
			*imagep = buf;
			*image_sizep = inout_size;
			break;
		}
		}

		return 0;
	}

	return 0;
}
