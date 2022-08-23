// SPDX-License-Identifier: GPL-2.0+
/*
 * Executes tests for fru command
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <common.h>
#include <console.h>
#include <mapmem.h>
#include <test/suites.h>
#include <test/ut.h>

#define FRU_TEST(_name, _flags)	UNIT_TEST(_name, _flags, fru_test)

#define CMD_FRU_TEST_SRC_BUF_SIZE 1024

static const char cmd_gen_b[] =
	"fru generate -b %08lx abcd efgh ijkl mnop qrst uvwx";
static const char cmd_gen_p[] =
	"fru generate -p %08lx abcd efgh ijkl mnop qrst uvwx yz01 2345";

static const char cmd_capture[] = "fru capture %08lx";

static int fru_test_board(struct unit_test_state *uts)
{
	u8 fru_src[CMD_FRU_TEST_SRC_BUF_SIZE];
	int i;

	ut_assertok(console_record_reset_enable());
	ut_assertok(run_commandf(cmd_gen_b, (ulong)map_to_sysmem(fru_src)));
	ut_assertok(run_commandf(cmd_capture, (ulong)map_to_sysmem(fru_src)));
	ut_assertok(run_command("fru display", 0));
	for (i = 0; i < 11; i++)
		ut_assert_skipline();
	ut_assert_nextline(" Manufacturer Name: abcd");
	ut_assert_nextline(" Product Name: efgh");
	ut_assert_nextline(" Serial Number: ijkl");
	ut_assert_nextline(" Part Number: mnop");
	ut_assert_nextline(" File ID: qrst");
	ut_assert_nextline(" Custom Type/Length: 0xc4");
	ut_assert_nextline("  00000000: 75 76 77 78                                      uvwx");
	for (i = 0; i < 4; i++)
		ut_assert_skipline();
	ut_assertok(ut_check_console_end(uts));

	return 0;
}
FRU_TEST(fru_test_board, UT_TESTF_CONSOLE_REC);

static int fru_test_product(struct unit_test_state *uts)
{
	u8 fru_src[CMD_FRU_TEST_SRC_BUF_SIZE];
	int i;

	ut_assertok(console_record_reset_enable());
	ut_assertok(run_commandf(cmd_gen_p, (ulong)map_to_sysmem(fru_src)));
	ut_assertok(run_commandf(cmd_capture, (ulong)map_to_sysmem(fru_src)));
	ut_assertok(run_command("fru display", 0));
	for (i = 0; i < 14; i++)
		ut_assert_skipline();
	ut_assert_nextline(" Manufacturer Name: abcd");
	ut_assert_nextline(" Product Name: efgh");
	ut_assert_nextline(" Part Number: ijkl");
	ut_assert_nextline(" Version Number: mnop");
	ut_assert_nextline(" Serial Number: qrst");
	ut_assert_nextline(" Asset Number: uvwx");
	ut_assert_nextline(" File ID: yz01");
	ut_assert_nextline(" Custom Type/Length: 0xc4");
	ut_assert_nextline("  00000000: 32 33 34 35                                      2345");
	ut_assert_skipline();
	ut_assertok(ut_check_console_end(uts));

	return 0;
}
FRU_TEST(fru_test_product, UT_TESTF_CONSOLE_REC);

int do_ut_fru(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct unit_test *tests = UNIT_TEST_SUITE_START(fru_test);
	const int n_ents = UNIT_TEST_SUITE_COUNT(fru_test);

	return cmd_ut_category("fru", "fru_test_", tests, n_ents, argc, argv);
}
