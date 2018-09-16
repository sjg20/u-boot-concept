// SPDX-License-Identifier: GPL-2.0+
/*
 * Power and reset support
 * Written by Simon Glass <sjg@chromium.org>
 *
 * Copyright (c) 2018 Google, Inc
 */

#include <common.h>
#include <log.h>
#include <sysreset.h>

void cold_reboot(void)
{
	sysreset_walk_halt(SYSRESET_WARM);
}

void power_off(void)
{
	sysreset_walk_halt(SYSRESET_POWER_OFF);
}

int is_processor_reset(void)
{
	int ret;

	ret = sysreset_get_last_walk();

	return ret == SYSRESET_POWER;
}
