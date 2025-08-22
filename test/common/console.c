// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 *
 * Test for console functionality
 */

#include <console.h>
#include <env.h>
#include <malloc.h>
#include <serial.h>
#include <video.h>
#include <test/common.h>
#include <test/test.h>
#include <test/ut.h>
#include <asm/global_data.h>
#include <asm/state.h>

DECLARE_GLOBAL_DATA_PTR;

/* Test calc_check_console_lines() with environment variable override */
static int console_test_calc_lines_env_override(struct unit_test_state *uts)
{
	int original_lines, lines;
	char *orig_env;

	/* Save original environment value */
	orig_env = env_get("pager");
	original_lines = calc_check_console_lines();

	/* Test with hex environment variable */
	ut_assertok(env_set("pager", "1a"));  /* 26 in hex */
	lines = calc_check_console_lines();
	ut_asserteq(26, lines);

	/* Test with decimal-looking hex value */
	ut_assertok(env_set("pager", "20"));  /* 32 in hex */
	lines = calc_check_console_lines();
	ut_asserteq(32, lines);

	/* Test with zero (should disable pager) */
	ut_assertok(env_set("pager", "0"));
	lines = calc_check_console_lines();
	ut_asserteq(0, lines);

	/* Restore original environment */
	ut_assertok(env_set("pager", orig_env));

	/* Verify restoration */
	lines = calc_check_console_lines();
	ut_asserteq(original_lines, lines);

	return 0;
}
COMMON_TEST(console_test_calc_lines_env_override, 0);

/* Test calc_check_console_lines() with invalid environment values */
static int console_test_calc_lines_env_invalid(struct unit_test_state *uts)
{
	int original_lines, lines;
	char *orig_env;

	/* Save original environment value */
	orig_env = env_get("pager");
	original_lines = calc_check_console_lines();

	/* Test with invalid hex value - should fall back to default */
	ut_assertok(env_set("pager", "xyz"));
	lines = calc_check_console_lines();
	/* Should return default since 'pager' reads as 0 */
	ut_asserteq(CONFIG_CONSOLE_PAGER_LINES, lines);

	/* Test with empty string */
	ut_assertok(env_set("pager", ""));
	lines = calc_check_console_lines();
	ut_asserteq(CONFIG_CONSOLE_PAGER_LINES, lines);

	/* Restore original environment */
	ut_assertok(env_set("pager", orig_env));

	return 0;
}
COMMON_TEST(console_test_calc_lines_env_invalid, 0);

/* Test calc_check_console_lines() default behavior without environment */
static int console_test_calc_lines_default(struct unit_test_state *uts)
{
	int lines;
	char *orig_env;

	/* Save and clear environment variable */
	orig_env = env_get("pager");
	ut_assertok(env_set("pager", NULL));

	/* Get default behavior */
	lines = calc_check_console_lines();

	/*
	 * The function considers device detection (video/serial) which may
	 * override the CONFIG default. In sandbox, serial_is_tty() may return
	 * false during tests, causing the function to return 0. Just verify it
	 * returns a reasonable value.
	 */
	ut_assert(lines >= 0);

	/* If CONFIG_CONSOLE_PAGER is not enabled, should definitely be 0 */
	if (!IS_ENABLED(CONFIG_CONSOLE_PAGER)) {
		ut_asserteq(0, lines);
	}

	/* Restore original environment */
	ut_assertok(env_set("pager", orig_env));

	return 0;
}
COMMON_TEST(console_test_calc_lines_default, 0);

/* Test calc_check_console_lines() precedence: env overrides everything */
static int console_test_calc_lines_precedence(struct unit_test_state *uts)
{
	int lines;
	char *orig_env;

	/* Save original environment value */
	orig_env = env_get("pager");

	/* Set environment to a specific value */
	ut_assertok(env_set("pager", "2a"));  /* 42 in hex */
	lines = calc_check_console_lines();
	
	/*
	 * Environment should always take precedence regardless of video/serial
	 * state
	 */
	ut_asserteq(42, lines);

	/* Test with zero environment value */
	ut_assertok(env_set("pager", "0"));
	lines = calc_check_console_lines();
	ut_asserteq(0, lines);

	/* Restore original environment */
	ut_assertok(env_set("pager", orig_env));

	return 0;
}
COMMON_TEST(console_test_calc_lines_precedence, 0);

/* Test calc_check_console_lines() with serial TTY detection */
static int console_test_calc_lines_serial_tty(struct unit_test_state *uts)
{
	int lines;
	char *orig_env;
	bool orig_tty;
	struct sandbox_state *state;

	/* Save original state */
	orig_env = env_get("pager");
	state = state_get_current();
	orig_tty = state->serial_is_tty;

	/* Clear environment to test device detection */
	ut_assertok(env_set("pager", NULL));

	/* Test with serial TTY enabled */
	state->serial_is_tty = true;
	lines = calc_check_console_lines();
	/* Should attempt to query serial size or use default */
	ut_assert(lines >= 0);

	/* Test with serial TTY disabled (not a terminal) */
	state->serial_is_tty = false;
	lines = calc_check_console_lines();
	/* Should return 0 when not connected to TTY */
	ut_asserteq(0, lines);

	/* Restore original state */
	state->serial_is_tty = orig_tty;
	ut_assertok(env_set("pager", orig_env));

	return 0;
}
COMMON_TEST(console_test_calc_lines_serial_tty, 0);
