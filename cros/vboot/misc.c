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
#include <gpt.h>
#include <vb2_api.h>
#include <cros/cros_common.h>
#include <cros/vboot_flag.h>

uint32_t VbExIsShutdownRequested(void)
{
	int ret, prev;

	/* if lid is NOT OPEN */
	ret = vboot_flag_read_walk(VBOOT_FLAG_LID_OPEN);
	if (ret == 0) {
		VB2_DEBUG("Lid-closed is detected.\n");
		return 1;
	}
	/*
	 * If power switch is pressed (but previously was known to be not
	 * pressed), we power off.
	 */
	ret = vboot_flag_read_walk_prev(VBOOT_FLAG_POWER_OFF, &prev);
	if (!ret && prev == 1) {
		VB2_DEBUG("Power-key-pressed is detected.\n");
		return 1;
	}
	/*
	 * Either the gpios don't exist, or the lid is up and and power button
	 * is not pressed. No-Shutdown-Requested.
	 */
	return 0;
}

uint8_t VbExOverrideGptEntryPriority(const GptEntry *e)
{
	return 0;
}
