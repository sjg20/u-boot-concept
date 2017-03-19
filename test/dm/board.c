/*
 * Copyright (C) 2015 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/state.h>
#include <asm/test.h>
#include <dm/test.h>
#include <test/ut.h>

DECLARE_GLOBAL_DATA_PTR;

/* Test invoking a board phase with three active devices */
static int dm_test_board(struct unit_test_state *uts)
{
	struct sandbox_state *state = state_get_current();

	/* We should start with a count of 0 for our test phase */
	ut_asserteq(0, gd->phase_count[BOARD_PHASE_TEST]);

	/* Check that we can detect there being no driver */
	ut_asserteq(-ENOSYS, board_walk_phase_count(BOARD_PHASE_INVALID,
						    false));
	ut_asserteq(0, board_walk_opt_phase(BOARD_PHASE_INVALID));
	ut_asserteq(-ENOSYS, board_walk_phase(BOARD_PHASE_INVALID));

	/* If no devices respond, we should get no action */
	state->board_sandbox_ret[BOARD_TEST0] = -ENOSYS;
	state->board_sandbox_ret[BOARD_TEST1] = -ENOSYS;
	state->board_sandbox_ret[BOARD_TEST2] = -ENOSYS;
	ut_asserteq(-ENOSYS, board_walk_phase_count(BOARD_PHASE_TEST, false));
	ut_asserteq(0, board_walk_opt_phase(BOARD_PHASE_TEST));
	ut_asserteq(0, gd->phase_count[BOARD_PHASE_TEST]);

	/* Enable the first device */
	state->board_sandbox_ret[BOARD_TEST0] = 0;
	ut_asserteq(1, board_walk_phase_count(BOARD_PHASE_TEST, false));
	ut_asserteq(1, gd->phase_count[BOARD_PHASE_TEST]);

	/* Enable the second device too */
	state->board_sandbox_ret[BOARD_TEST1] = 0;
	ut_asserteq(2, board_walk_phase_count(BOARD_PHASE_TEST, false));
	ut_asserteq(3, gd->phase_count[BOARD_PHASE_TEST]);

	/* Enable all three devices */
	state->board_sandbox_ret[BOARD_TEST2] = 0;
	ut_asserteq(3, board_walk_phase_count(BOARD_PHASE_TEST, false));
	ut_asserteq(6, gd->phase_count[BOARD_PHASE_TEST]);

	/*
	 * Check that the first device can claim the phase and lock out the
	 * other devices.
	 */
	state->board_sandbox_ret[BOARD_TEST0] = BOARD_PHASE_CLAIMED;
	ut_asserteq(1, board_walk_phase_count(BOARD_PHASE_TEST, false));
	ut_asserteq(0, board_walk_phase(BOARD_PHASE_TEST));
	ut_asserteq(0, board_walk_opt_phase(BOARD_PHASE_TEST));
	ut_asserteq(9, gd->phase_count[BOARD_PHASE_TEST]);

	/* Any error should be reported, but previous devices should still get
	 * to process the phase.
	 */
	state->board_sandbox_ret[BOARD_TEST0] = 0;
	state->board_sandbox_ret[BOARD_TEST1] = -ENOENT;
	ut_asserteq(-ENOENT, board_walk_phase_count(BOARD_PHASE_TEST, false));
	ut_asserteq(-ENOENT, board_walk_phase(BOARD_PHASE_TEST));
	ut_asserteq(-ENOENT, board_walk_opt_phase(BOARD_PHASE_TEST));
	ut_asserteq(12, gd->phase_count[BOARD_PHASE_TEST]);

	return 0;
}
DM_TEST(dm_test_board, DM_TESTF_SCAN_FDT);
