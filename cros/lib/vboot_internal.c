// SPDX-License-Identifier: GPL-2.0+
/*
 * Holds functions that need access to internal vboot data
 *
 * Copyright (c) 2018 Google, Inc
 */

#define NEED_VB20_INTERNALS

#include <common.h>
#include <cros/vboot.h>

/* Read the BOOT_OPROM_NEEDED flag from VBNV. */
int vboot_wants_oprom(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);

	return (ctx->nvdata[VB2_NV_OFFS_BOOT] & VB2_NV_BOOT_OPROM_NEEDED) ? 1 : 0;
}
