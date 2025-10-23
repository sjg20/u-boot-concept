// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for bootstage command
 *
 * Copyright 2025 Canonical Ltd
 */

#include <bootstage.h>
#include <test/cmd.h>
#include <test/ut.h>

static int cmd_bootstage_report(struct unit_test_state *uts)
{
	uint count;

	/* Get the current record count */
	count = bootstage_get_rec_count();
	ut_assert(count > 0);

	/* Test the bootstage report command runs successfully */
	ut_assertok(run_command("bootstage report", 0));

	/* Verify the report contains expected headers and stages */
	ut_assert_nextline("Timer summary in microseconds (%u records):",
			   count);
	ut_assert_nextline("       Mark    Elapsed  Stage");
	ut_assert_nextline("          0          0  reset");
	ut_assert_skip_to_line("Accumulated time:");

	return 0;
}
CMD_TEST(cmd_bootstage_report, UTF_CONSOLE);
