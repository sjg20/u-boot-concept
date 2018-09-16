/*
 * Copyright (c) 2017 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <cros/vboot.h>

int vboot_ver3_try_fw(struct vboot_info *vboot)
{
	int ret;

	bootstage_mark(BOOTSTAGE_VBOOT_START_VERIFY_SLOT);
	ret = vb2api_fw_phase3(vboot_get_ctx(vboot));
	bootstage_mark(BOOTSTAGE_VBOOT_END_VERIFY_SLOT);
	if (ret) {
		vboot_log(LOGL_INFO, "Reboot reqested (%x)\n", ret);
		return VBERROR_REBOOT_REQUIRED;
	}

	return 0;
}
