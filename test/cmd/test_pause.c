// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for pause command
 *
 * Copyright 2022, Samuel Dionne-Riel <samuel@dionne-riel.com>
 *
 * Based on tests for echo:
 * Copyright 2020, Heinrich Schuchadt <xypron.glpk@gmx.de>
 */

#include <common.h>
#include <command.h>
#include <asm/global_data.h>
#include <display_options.h>
#include <test/lib.h>
#include <test/test.h>
#include <test/ut.h>

DECLARE_GLOBAL_DATA_PTR;

struct test_data {
	char *cmd;
	char *expected;
	int expected_ret;
};

static struct test_data pause_data[] = {
	/* Test default message */
	{"pause",
	 "Press any key to continue...",
	 0},
	/* Test provided message */
	{"pause 'Prompt for pause...'",
	 "Prompt for pause...",
	 0},
	/* Test providing more than one params */
	{"pause a b",
	 "pause - delay until user input", /* start of help message */
	 1},
};

static int lib_test_hush_pause(struct unit_test_state *uts)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pause_data); ++i) {
		ut_silence_console(uts);
		console_record_reset_enable();
		/* Only cook a newline when the command is expected to pause. */
		if (pause_data[i].expected_ret == 0)
			console_in_puts("\n");
		ut_asserteq(pause_data[i].expected_ret, run_command(pause_data[i].cmd, 0));
		ut_unsilence_console(uts);
		console_record_readline(uts->actual_str,
					sizeof(uts->actual_str));
		ut_asserteq_str(pause_data[i].expected, uts->actual_str);
		ut_asserteq(pause_data[i].expected_ret, ut_check_console_end(uts));
	}
	return 0;
}

LIB_TEST(lib_test_hush_pause, 0);
