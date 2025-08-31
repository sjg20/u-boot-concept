// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for chid command
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <test/cmd.h>
#include <test/ut.h>
#include <version.h>

/* Test the 'chid show' command */
static int cmd_chid_show_test(struct unit_test_state *uts)
{
	/* Test chid show command and verify expected output */
	ut_assertok(run_command("chid show", 0));

	ut_assert_nextline("Manufacturer:      Sandbox Corp");
	ut_assert_nextline("Family:            Sandbox_Family");
	ut_assert_nextline("Product Name:      Sandbox Computer");
	ut_assert_nextline("Product SKU:       SANDBOX-SKU");
	ut_assert_nextline("Baseboard Manuf:   Sandbox Boards");
	ut_assert_nextline("Baseboard Product: Sandbox Motherboard");
	ut_assert_nextline("BIOS Vendor:       U-Boot");
	ut_assert_nextlinen("BIOS Version:      " PLAIN_VERSION);
	ut_assert_nextline("BIOS Major:        %u", U_BOOT_VERSION_NUM % 100);
	ut_assert_nextline("BIOS Minor:        %u", U_BOOT_VERSION_NUM_PATCH);
	ut_assert_nextline("Enclosure Type:    2");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_chid_show_test, UTF_CONSOLE);

/* Test invalid chid subcommand */
static int cmd_chid_invalid_test(struct unit_test_state *uts)
{
	/* Test chid command with invalid arguments */
	ut_asserteq(1, run_command("chid invalid", 0));

	return 0;
}
CMD_TEST(cmd_chid_invalid_test, UTF_CONSOLE);
