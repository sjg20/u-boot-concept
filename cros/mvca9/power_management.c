/*
 * Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

/* Implementation of per-board power management function */

#include <common.h>
#include <cros/common.h>
#include <cros/power_management.h>

int is_processor_reset(void)
{
	/* TODO(sjg@chromium.org): add board-specific code */
	return 1;
}

/* This function never returns */
void cold_reboot(void)
{
	printf("Please press cold reboot button\n");
	while (1)
		;
}


/* This function never returns */
void power_off(void)
{
	printf("Please unplug the power cable and battery\n");
	while (1)
		;
}
