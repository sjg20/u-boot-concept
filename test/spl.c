// SPDX-License-Identifier: GPL-2.0+

#include <common.h>
#include <test/ut.h>

#define SPL_TEST(_name, _flags)	UNIT_TEST(_name, _flags, spl_test)

/* Test 'setexpr' command with simply setting integers */
static int spl_phase_test(struct unit_test_state *uts)
{
	return 0;
}
SPL_TEST(setexpr_test_int, 0);

int spl_test_main(struct unit_test_state *uts)
{
	struct unit_test *tests = ll_entry_start(struct unit_test, dm_test);
	const int n_ents = ll_entry_count(struct unit_test, dm_test);
	struct unit_test_state *uts = &global_dm_test_state;


}
