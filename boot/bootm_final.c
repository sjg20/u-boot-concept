/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Cleanup before handing off to the OS
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <bootm.h>
#include <bootstage.h>
#include <dm.h>
#include <dm/device-internal.h>

void bootm_final(void)
{
	printf("\nStarting kernel ...\n\n");

	bootstage_mark_name(BOOTSTAGE_ID_BOOTM_HANDOFF, "start_kernel");
#if IS_ENABLED(CONFIG_BOOTSTAGE_REPORT)
	bootstage_report();
#endif

	dm_remove_devices_active();
}
