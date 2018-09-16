/*
 * Copyright (c) 2017 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <log.h>
#include <cros/vboot.h>

int vboot_ver1_vbinit(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	int ret;

	ret = vb2api_fw_phase1(ctx);
	if (ret) {
		vboot_log(LOGL_WARNING, "ret=%x\n", ret);
		/*
		 * If vb2api_fw_phase1 fails, check for return value.
		 * If it is set to VB2_ERROR_API_PHASE1_RECOVERY, then continue
		 * into recovery mode.
		 * For any other error code, save context if needed and reboot.
		 */
		if (ret == VB2_ERROR_API_PHASE1_RECOVERY) {
			vboot_log(LOGL_WARNING, "Recovery requested (%x)\n", ret);
			extend_pcrs(vboot);	/* ignore failures */
			bootstage_mark(BOOTSTAGE_VBOOT_END);
			return ret;
		}

		vboot_log(LOGL_WARNING, "Reboot reqested (%x)\n", ret);
		return VBERROR_REBOOT_REQUIRED;
	}

	return 0;
}
