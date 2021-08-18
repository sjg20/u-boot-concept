// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <bootmethod.h>
#include <test/suites.h>
#include <test/ut.h>

/* Declare a new bootmethod test */
#define BOOTMETHOD_TEST(_name, _flags) \
		UNIT_TEST(_name, _flags, bootmethod_test)

/* Check 'bootmethod list' command */
static int bootmethod_test_cmd_list(struct unit_test_state *uts)
{
	int probed;

	console_record_reset_enable();
	for (probed = 0; probed < 2; probed++) {
		int probe_ch = probed ? '+' : ' ';

		ut_assertok(run_command(probed ? "bootmethod list -p" :
			"bootmethod list", 0));
		ut_assert_nextline("Seq  Probed  Status  Uclass    Name");
		ut_assert_nextlinen("---");
		ut_assert_nextline("%3x   [ %c ]  %6s  %-8s  %s", 0, probe_ch, "OK",
				   "mmc", "mmc2.bootmethod");
		ut_assert_nextline("%3x   [ %c ]  %6s  %-8s  %s", 1, probe_ch, "OK",
				   "mmc", "mmc1.bootmethod");
		ut_assert_nextline("%3x   [ %c ]  %6s  %-8s  %s", 2, probe_ch, "OK",
				   "mmc", "mmc0.bootmethod");
		ut_assert_nextlinen("---");
		ut_assert_nextline("(3 devices)");
		ut_assert_console_end();
	}

	return 0;
}
BOOTMETHOD_TEST(bootmethod_test_cmd_list, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootmethod select' and 'info' commands */
static int bootmethod_test_cmd_select(struct unit_test_state *uts)
{
	console_record_reset_enable();
	ut_asserteq(1, run_command("bootmethod info", 0));
	ut_assert_nextlinen("Please use");
	ut_assert_console_end();

	ut_assertok(run_command("bootmethod select 0", 0));
	ut_assert_console_end();

	ut_assertok(run_command("bootmethod info", 0));
	ut_assert_nextline("Name:      mmc2.bootmethod");
	ut_assert_nextline("Uclass:    mmc");
	ut_assert_console_end();

	return 0;
}
BOOTMETHOD_TEST(bootmethod_test_cmd_select, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootflow scan/list' commands */
static int bootmethod_test_cmd_bootflow(struct unit_test_state *uts)
{
	console_record_reset_enable();
	ut_assertok(run_command("bootmethod select 2", 0));
	ut_assert_console_end();
	ut_assertok(run_command("bootflow scan -l", 0));
	ut_assert_nextline("Scanning for bootflows in bootmethod 'mmc0.bootmethod'");
	ut_assert_nextline("Seq  Type         State   Uclass    Part  Name                      Filename");
	ut_assert_nextlinen("---");
	ut_assert_nextline("  0  distro-boot  loaded  mmc          1  mmc0.bootmethod.part_1    extlinux/extlinux.conf");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(21 bootflows, 1 valid)");
	ut_assert_console_end();

	ut_assertok(run_command("bootflow list", 0));
	ut_assert_nextline("Showing bootflows for bootmethod 'mmc0.bootmethod'");
	ut_assert_nextline("Seq  Type         State   Uclass    Part  Name                      Filename");
	ut_assert_nextlinen("---");
	ut_assert_nextline("  0  distro-boot  loaded  mmc          1  mmc0.bootmethod.part_1    extlinux/extlinux.conf");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(1 bootflow, 1 valid)");
	ut_assert_console_end();

	return 0;
}
BOOTMETHOD_TEST(bootmethod_test_cmd_bootflow, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootflow scan/list' commands using all bootmethods */
static int bootmethod_test_cmd_bootflow_glob(struct unit_test_state *uts)
{
	console_record_reset_enable();
	ut_assertok(run_command("bootflow scan -l", 0));
	ut_assert_nextline("Scanning for bootflows in all bootmethods");
	ut_assert_nextline("Seq  Type         State   Uclass    Part  Name                      Filename");
	ut_assert_nextlinen("---");
	ut_assert_nextline("Scanning bootmethod 'mmc2.bootmethod':");
	ut_assert_nextline("Scanning bootmethod 'mmc1.bootmethod':");
	ut_assert_nextline("Scanning bootmethod 'mmc0.bootmethod':");
	ut_assert_nextline("  0  distro-boot  loaded  mmc          1  mmc0.bootmethod.part_1    extlinux/extlinux.conf");
	ut_assert_nextline("No more bootmethods");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(1 bootflow, 1 valid)");
	ut_assert_console_end();

	ut_assertok(run_command("bootflow list", 0));
	ut_assert_nextline("Showing all bootflows");
	ut_assert_nextline("Seq  Type         State   Uclass    Part  Name                      Filename");
	ut_assert_nextlinen("---");
	ut_assert_nextline("  0  distro-boot  loaded  mmc          1  mmc0.bootmethod.part_1    extlinux/extlinux.conf");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(1 bootflow, 1 valid)");
	ut_assert_console_end();

	return 0;
}
BOOTMETHOD_TEST(bootmethod_test_cmd_bootflow_glob,
		UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootflow scan -e' */
static int bootmethod_test_cmd_bootflow_scan_e(struct unit_test_state *uts)
{
	console_record_reset_enable();
	ut_assertok(run_command("bootflow scan -ale", 0));
	ut_assert_nextline("Scanning for bootflows in all bootmethods");
	ut_assert_nextline("Seq  Type         State   Uclass    Part  Name                      Filename");
	ut_assert_nextlinen("---");
	ut_assert_nextline("Scanning bootmethod 'mmc2.bootmethod':");
	ut_assert_nextline("  0  distro-boot  media   mmc          0  mmc2.bootmethod.part_1    <NULL>");
	ut_assert_nextline("     ** No partition found, err=-93");
	ut_assert_nextline("  1  distro-boot  media   mmc          0  mmc2.bootmethod.part_2    <NULL>");

	ut_assert_skip_to_line("Scanning bootmethod 'mmc1.bootmethod':");
	ut_assert_skip_to_line("Scanning bootmethod 'mmc0.bootmethod':");
	ut_assert_nextline(" 28  distro-boot  loaded  mmc          1  mmc0.bootmethod.part_1    extlinux/extlinux.conf");
	ut_assert_nextline(" 29  distro-boot  media   mmc          0  mmc0.bootmethod.part_2    <NULL>");
	ut_assert_skip_to_line(" 3b  distro-boot  media   mmc          0  mmc0.bootmethod.part_14   <NULL>");
	ut_assert_nextline("     ** No partition found, err=-2");
	ut_assert_nextline("No more bootmethods");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(60 bootflows, 1 valid)");
	ut_assert_console_end();

	ut_assertok(run_command("bootflow list", 0));
	ut_assert_nextline("Showing all bootflows");
	ut_assert_nextline("Seq  Type         State   Uclass    Part  Name                      Filename");
	ut_assert_nextlinen("---");
	ut_assert_nextline("  0  distro-boot  media   mmc          0  mmc2.bootmethod.part_1    <NULL>");
	ut_assert_skip_to_line(" 28  distro-boot  loaded  mmc          1  mmc0.bootmethod.part_1    extlinux/extlinux.conf");
	ut_assert_skip_to_line(" 3b  distro-boot  media   mmc          0  mmc0.bootmethod.part_14   <NULL>");
	ut_assert_nextlinen("---");
	ut_assert_nextline("(60 bootflows, 1 valid)");
	ut_assert_console_end();

	return 0;
}
BOOTMETHOD_TEST(bootmethod_test_cmd_bootflow_scan_e,
		UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootflow info' */
static int bootmethod_test_cmd_bootflow_info(struct unit_test_state *uts)
{
	console_record_reset_enable();
	ut_assertok(run_command("bootmethod select 2", 0));
	ut_assert_console_end();
	ut_assertok(run_command("bootflow scan", 0));
	ut_assert_console_end();
	ut_assertok(run_command("bootflow select 0", 0));
	ut_assert_console_end();
	ut_assertok(run_command("bootflow info", 0));
	ut_assert_nextline("Name:      mmc0.bootmethod.part_1");
	ut_assert_nextline("Device:    mmc0.bootmethod");
	ut_assert_nextline("Block dev: mmc0.blk");
	ut_assert_nextline("Sequence:  0");
	ut_assert_nextline("Type:      distro-boot");
	ut_assert_nextline("State:     loaded");
	ut_assert_nextline("Partition: 1");
	ut_assert_nextline("Subdir:    (none)");
	ut_assert_nextline("Filename:  extlinux/extlinux.conf");
	ut_assert_nextlinen("Buffer:    ");
	ut_assert_nextline("Size:      237 (567 bytes)");
	ut_assert_nextline("Error:     0");
	ut_assert_console_end();

	ut_assertok(run_command("bootflow info -d", 0));
	ut_assert_nextline("Name:      mmc0.bootmethod.part_1");
	ut_assert_skip_to_line("Error:     0");
	ut_assert_nextline("Contents:");
	ut_assert_nextline("%s", "");
	ut_assert_nextline("# extlinux.conf generated by appliance-creator");
	ut_assert_skip_to_line("initrd /initramfs-5.3.7-301.fc31.armv7hl.img");
	ut_assert_console_end();

	return 0;
}
BOOTMETHOD_TEST(bootmethod_test_cmd_bootflow_info,
		UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootflow scan -b' to boot the first available bootmethod */
static int bootmethod_test_cmd_bootflow_scan_boot(struct unit_test_state *uts)
{
	console_record_reset_enable();
	ut_assertok(run_command("bootflow scan -b", 0));
	ut_assert_nextline("** Booting bootflow 'mmc0.bootmethod.part_1'");
	ut_assert_nextline("Ignoring unknown command: ui");

	/*
	 * We expect it to get through to boot although sandbox always returns
	 * -EFAULT as it cannot actually boot the kernel
	 */
	ut_assert_skip_to_line("sandbox: continuing, as we cannot run Linux");
	ut_assert_nextline("Boot failed (err=-14)");
	ut_assert_console_end();

	return 0;
}
BOOTMETHOD_TEST(bootmethod_test_cmd_bootflow_scan_boot,
		UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootflow boot' to boot a selected bootflow */
static int bootmethod_test_cmd_bootflow_boot(struct unit_test_state *uts)
{
	console_record_reset_enable();
	ut_assertok(run_command("bootmethod select 2", 0));
	ut_assert_console_end();
	ut_assertok(run_command("bootflow scan", 0));
	ut_assert_console_end();
	ut_assertok(run_command("bootflow select 0", 0));
	ut_assert_console_end();
	ut_assertok(run_command("bootflow boot", 0));
	ut_assert_nextline("** Booting bootflow 'mmc0.bootmethod.part_1'");
	ut_assert_nextline("Ignoring unknown command: ui");

	/*
	 * We expect it to get through to boot although sandbox always returns
	 * -EFAULT as it cannot actually boot the kernel
	 */
	ut_assert_skip_to_line("sandbox: continuing, as we cannot run Linux");
	ut_assert_nextline("Boot failed (err=-14)");
	ut_assert_console_end();

	return 0;
}
BOOTMETHOD_TEST(bootmethod_test_cmd_bootflow_boot,
		UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check we can get a bootmethod */
static int bootmethod_test_get(struct unit_test_state *uts)
{
	struct bootmethod_iter iter;
	struct bootflow bflow;

	ut_assertok(bootmethod_scan_first_bootflow(&iter, 0, &bflow));

	return 0;
}
BOOTMETHOD_TEST(bootmethod_test_get, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

int do_ut_bootmethod(struct cmd_tbl *cmdtp, int flag, int argc,
		     char *const argv[])
{
	struct unit_test *tests = UNIT_TEST_SUITE_START(bootmethod_test);
	const int n_ents = UNIT_TEST_SUITE_COUNT(bootmethod_test);

	return cmd_ut_category("bootmethod", "bootmethod_test_",
			       tests, n_ents, argc, argv);
}
