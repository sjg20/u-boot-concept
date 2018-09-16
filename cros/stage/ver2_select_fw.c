/*
 * Copyright (c) 2017 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <cros/vboot.h>

int vboot_ver2_select_fw(struct vboot_info *vboot)
{
	int ret;

	ret = vb2api_fw_phase2(vboot_get_ctx(vboot));
	if (ret) {
		vboot_log(LOGL_INFO, "Reboot requested (%x)\n", ret);
		return VBERROR_REBOOT_REQUIRED;
	}

	return 0;
}
