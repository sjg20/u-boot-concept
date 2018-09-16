/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#include <common.h>
#include <config.h>
#include <tpm-v1.h>
#include <cros/vboot.h>

VbError_t VbExTpmInit(void)
{
	/* tpm_lite lib doesn't call VbExTpmOpen after VbExTpmInit. */
	return VbExTpmOpen();
}

VbError_t VbExTpmClose(void)
{
	struct vboot_info *vboot = vboot_get();

	if (tpm_close(vboot->tpm))
		return VBERROR_UNKNOWN;

	return VBERROR_SUCCESS;
}

VbError_t VbExTpmOpen(void)
{
	struct vboot_info *vboot = vboot_get();

	if (tpm_open(vboot->tpm))
		return VBERROR_UNKNOWN;

	return VBERROR_SUCCESS;
}

VbError_t VbExTpmSendReceive(const uint8_t* request, uint32_t request_length,
		uint8_t* response, uint32_t* response_length)
{
	struct vboot_info *vboot = vboot_get();
	size_t resp_len = *response_length;
	int ret;

	ret = tpm_xfer(vboot->tpm, request, request_length, response, &resp_len);
	*response_length = resp_len;
	if (ret)
		return VBERROR_UNKNOWN;

	return VBERROR_SUCCESS;
}
