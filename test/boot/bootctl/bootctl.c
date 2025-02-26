// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for bootctl
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#include <bootctl.h>
#include <test/ut.h>
#include "bootctl_common.h"

static int bootctl_base(struct unit_test_state *uts)
{
	return 0;
}
BOOTCTL_TEST(bootctl_base, UTF_DM | UTF_SCAN_FDT | UTF_CONSOLE);
