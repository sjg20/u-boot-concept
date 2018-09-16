/*
 * Copyright (c) 2018 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <cros/vboot.h>

int vboot_ver6_jump_fw(struct vboot_info *vboot)
{
	struct fmap_entry *entry;
	int ret;

	entry = &vboot->blob->spl_entry;
	ret = fwstore_jump(vboot, entry->offset, vboot->fw_size);
	if (ret)
		return log_msg_ret("Jump via fwstore", ret);

	return 0;
}
