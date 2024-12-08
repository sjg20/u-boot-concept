// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for 'part_find' command
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/ofnode.h>
#include <dm/test.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Test 'addr_find' command */
static int cmd_test_addr_find(struct unit_test_state *uts)
{
	ut_assertok(env_set("loadaddr", NULL));
	ut_assertok(run_command("addr_find mmc 1:1 vmlinuz-5.3.7-301.fc31.armv7hl", 0));
	ut_assert_console_end();

	ut_assertnonnull(env_get("loadaddr"));

	return 0;
}
CMD_TEST(cmd_test_addr_find, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);
