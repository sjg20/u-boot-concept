// SPDX-License-Identifier: GPL-2.0+
/*
 * Cleanup before handing off to the OS
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <bootm.h>
#include <bootstage.h>
#include <dm/root.h>

void bootm_final(enum bootm_final_t flags)
{
	printf("\nStarting kernel ...\n\n");

	bootstage_mark_name(BOOTSTAGE_ID_BOOTM_HANDOFF, "start_kernel");

	if (IS_ENABLED(CONFIG_BOOTSTAGE_REPORT))
		bootstage_report();

	/*
	 * Call remove function of all devices with a removal flag set.
	 * This may be useful for last-stage operations, like cancelling
	 * of DMA operation or releasing device internal buffers.
	 */
	dm_remove_devices_active();
}
