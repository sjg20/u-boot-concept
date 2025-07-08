/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Cleanup before handing off to the OS
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <bootm.h>
#include <dm.h>
#include <dm/device-internal.h>

void bootm_final(void)
{
	printf("\nStarting kernel ...\n\n");

	dm_remove_devices_active();
}
