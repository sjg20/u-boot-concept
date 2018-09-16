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
#include <cros/vboot.h>

#include <gbb_header.h>

int gbb_init(struct vboot_info *vboot, void *gbb, uint32_t gbb_offset,
	     size_t gbb_size)
{
	GoogleBinaryBlockHeader *gbbh = (GoogleBinaryBlockHeader *)gbb;
	uint32_t hwid_end;
	uint32_t rootkey_end;
	int ret;

	ret = resource_read(vboot, VB2_RES_GBB, 0, gbbh, sizeof(*gbbh));
	if (ret)
		return log_msg_ret("failed to read GBB header\n", ret);

	hwid_end = gbbh->hwid_offset + gbbh->hwid_size;
	rootkey_end = gbbh->rootkey_offset + gbbh->rootkey_size;
	if (hwid_end < gbbh->hwid_offset || hwid_end > gbb_size ||
			rootkey_end < gbbh->rootkey_offset ||
			rootkey_end > gbb_size) {
		VB2_DEBUG("%s: invalid gbb header entries\n", __func__);
		VB2_DEBUG("   hwid_end=%x\n", hwid_end);
		VB2_DEBUG("   gbbh->hwid_offset=%x\n", gbbh->hwid_offset);
		VB2_DEBUG("   gbb_size=%zx\n", gbb_size);
		VB2_DEBUG("   rootkey_end=%x\n", rootkey_end);
		VB2_DEBUG("   gbbh->rootkey_offset=%x\n",
			gbbh->rootkey_offset);
		VB2_DEBUG("   %d, %d, %d, %d\n",
			hwid_end < gbbh->hwid_offset,
			hwid_end >= gbb_size,
			rootkey_end < gbbh->rootkey_offset,
			rootkey_end >= gbb_size);
		return 1;
	}

	ret = resource_read(vboot, VB2_RES_GBB, gbbh->hwid_offset,
			    gbb + gbbh->hwid_offset, gbbh->hwid_size);
	if (ret)
		return log_msg_ret("failed to read HWID\n", ret);

	ret = resource_read(vboot, VB2_RES_GBB, gbbh->rootkey_offset,
			    gbb + gbbh->rootkey_offset, gbbh->rootkey_size);
	if (ret)
		return log_msg_ret("failed to read root key\n", ret);
	vboot->gbb_flags = gbbh->flags;

	return 0;
}

int gbb_read_bmp_block(struct vboot_info *vboot, void *gbb, uint32_t gbb_offset,
		       size_t gbb_size)
{
	GoogleBinaryBlockHeader *gbbh = (GoogleBinaryBlockHeader *)gbb;
	uint32_t bmpfv_end = gbbh->bmpfv_offset + gbbh->bmpfv_size;
	int ret;

	if (bmpfv_end < gbbh->bmpfv_offset || bmpfv_end > gbb_size) {
		VB2_DEBUG("%s: invalid gbb header entries, bmpfv_end=%x, gbbh->bmpfv_offset=%x, gbb_size=%x\n",
			__func__, bmpfv_end, gbbh->bmpfv_offset, gbb_size);
		return 1;
	}

	ret = resource_read(vboot, VB2_RES_GBB, gbbh->bmpfv_offset,
			    gbb + gbbh->bmpfv_offset, gbbh->bmpfv_size);
	if (ret)
		return log_msg_ret("failed to read bmp block\n", ret);

	return 0;
}

int gbb_read_recovery_key(struct vboot_info *vboot, void *gbb,
			  uint32_t gbb_offset, size_t gbb_size)
{
	GoogleBinaryBlockHeader *gbbh = (GoogleBinaryBlockHeader *)gbb;
	uint32_t rkey_end = gbbh->recovery_key_offset +
		gbbh->recovery_key_size;
	int ret;

	if (rkey_end < gbbh->recovery_key_offset || rkey_end > gbb_size) {
		VB2_DEBUG("%s: invalid gbb header entries\n", __func__);
		VB2_DEBUG("   gbbh->recovery_key_offset=%x\n",
			gbbh->recovery_key_offset);
		VB2_DEBUG("   gbbh->recovery_key_size=%x\n",
			gbbh->recovery_key_size);
		VB2_DEBUG("   rkey_end=%x\n", rkey_end);
		VB2_DEBUG("   gbb_size=%zx\n", gbb_size);
		return 1;
	}

	ret = resource_read(vboot, VB2_RES_GBB, gbbh->recovery_key_offset,
			    gbb + gbbh->recovery_key_offset,
		     gbbh->recovery_key_size);
	if (ret)
		return log_msg_ret("failed to read recovery key\n", ret);

	return 0;
}

uint32_t gbb_get_flags(struct vboot_info *vboot)
{
	return vboot->gbb_flags;
}

int gbb_check_integrity(uint8_t *gbb)
{
	/*
	 * Avoid a second "$GBB" signature in the binary. Some utility programs
	 * that parses the contents of firmware image could fail if there are
	 * multiple signatures.
	 */
	if (gbb[0] == '$' && gbb[1] == 'G' && gbb[2] == 'B' && gbb[3] == 'B')
		return 0;
	else
		return 1;
}
