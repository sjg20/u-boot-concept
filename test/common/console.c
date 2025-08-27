// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 *
 * Test for console functionality
 */

#include <console.h>
#include <dm/uclass.h>
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

	/* save original environment value */
	orig_env = env_get("pager");
	original_lines = calc_check_console_lines();

	ut_assertok(env_set("pager", "1a"));
	lines = calc_check_console_lines();
	ut_asserteq(0x1a, lines);

	/* decimal-looking hex value */
	ut_assertok(env_set("pager", "20"));  /* 32 in hex */
	lines = calc_check_console_lines();
	ut_asserteq(0x20, lines);

	/* zero - should disable pager */
	ut_assertok(env_set("pager", "0"));
	lines = calc_check_console_lines();
	ut_asserteq(0, lines);

	/* restore original environment */
	ut_assertok(env_set("pager", orig_env));

	/* verify restoration */
	lines = calc_check_console_lines();
	ut_asserteq(original_lines, lines);

	return 0;
}
COMMON_TEST(console_test_calc_lines_env_override, 0);

/* Test calc_check_console_lines() with invalid environment values */
static int console_test_calc_lines_env_invalid(struct unit_test_state *uts)
{
	struct sandbox_state *state;
	char *orig_env;
	bool orig_tty;
	int lines;

	/* save original state */
	orig_env = env_get("pager");
	state = state_get_current();
	orig_tty = state->serial_is_tty;

	/* invalid hex value and no terminal - should return 0 */
	state->serial_is_tty = false;
	ut_assertok(env_set("pager", "xyz"));
	lines = calc_check_console_lines();
	ut_asserteq(0, lines);

	/* empty string and no terminal - should return 0 */
	ut_assertok(env_set("pager", ""));
	lines = calc_check_console_lines();
	ut_asserteq(0, lines);

	/* invalid hex value and terminal - should fall back to CONFIG */
	state->serial_is_tty = true;
	ut_assertok(env_set("pager", "xyz"));
	lines = calc_check_console_lines();
	ut_asserteq(CONFIG_CONSOLE_PAGER_LINES, lines);

	/* restore original state */
	state->serial_is_tty = orig_tty;
	ut_assertok(env_set("pager", orig_env));

	return 0;
}
COMMON_TEST(console_test_calc_lines_env_invalid, 0);

/* Test calc_check_console_lines() default behavior without environment */
static int console_test_calc_lines_default(struct unit_test_state *uts)
{
	struct sandbox_state *state;
	struct serial_priv *priv;
	struct uclass *uc;
	int lines;
	char *orig_env;
	bool orig_tty;

	/* Save original state */
	orig_env = env_get("pager");
	state = state_get_current();
	orig_tty = state->serial_is_tty;

	/* Clear environment variable */
	ut_assertok(env_set("pager", NULL));

	/* no terminal - should return 0 (pager disabled) */
	state->serial_is_tty = false;
	lines = calc_check_console_lines();
	ut_asserteq(0, lines);

	/* terminal enabled but no cached size - returns CONFIG default */
	state->serial_is_tty = true;
	lines = calc_check_console_lines();
	ut_asserteq(CONFIG_CONSOLE_PAGER_LINES, lines);

	/* set cached serial size and verify it's used */
	ut_assertok(uclass_get(UCLASS_SERIAL, &uc));
	priv = uclass_get_priv(uc);
	priv->rows = 30;
	priv->cols = 80;
	lines = calc_check_console_lines();
	ut_asserteq(30, lines);

	/* Clear cached values for cleanup */
	priv->rows = 0;
	priv->cols = 0;

	/* Restore original state */
	state->serial_is_tty = orig_tty;
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
	ut_assertok(env_set("pager", "2a"));
	lines = calc_check_console_lines();

	/*
	 * Environment should always take precedence regardless of video/serial
	 * state
	 */
	ut_asserteq(0x2a, lines);

	/* Test with zero environment value */
	ut_assertok(env_set("pager", "0"));
	lines = calc_check_console_lines();
	ut_asserteq(0, lines);

	/* Restore original environment */
	ut_assertok(env_set("pager", orig_env));

	return 0;
}
COMMON_TEST(console_test_calc_lines_precedence, 0);

/* Test calc_check_console_lines() with serial terminal detection */
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

	/* Test with serial terminal enabled */
	state->serial_is_tty = true;
	lines = calc_check_console_lines();
	/* Should attempt to query serial size or use default */
	ut_assert(lines >= 0);

	/* Test with serial terminal disabled (not a terminal) */
	state->serial_is_tty = false;
	lines = calc_check_console_lines();
	/* Should return 0 when not connected to terminal */
	ut_asserteq(0, lines);

	/* Restore original state */
	state->serial_is_tty = orig_tty;
	ut_assertok(env_set("pager", orig_env));

	return 0;
}
COMMON_TEST(console_test_calc_lines_serial_tty, 0);
