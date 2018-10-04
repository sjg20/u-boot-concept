// SPDX-License-Identifier: GPL-2.0
/*
 * Jumping to U-Boot
 *
 * Copyright (c) 2018 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bloblist.h>
#include <cros/vboot.h>

int vboot_spl_jump_u_boot(struct vboot_info *vboot)
{
	struct fmap_entry *entry;
	int ret;

	/* TODO(sjg@chromium.org): Verify the hash here */
	bloblist_finish();
	entry = &vboot->blob->u_boot_entry;
	ret = fwstore_jump(vboot, entry);
	if (ret)
		return log_msg_ret("Jump via fwstore", ret);

	return 0;
}
