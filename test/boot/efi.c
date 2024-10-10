// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for EFI-specific booting
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <test/cmd.h>
#include <test/suites.h>
#include <test/ut.h>

/* Check "bootdev list" command */
static int test_efi_bootmgr_norun(struct unit_test_state *uts)
{
	ut_assertok(run_command("efidebug boot add -b 0001 label-1 host 0:1 "
		"initrddump.efi -i host 0:1 initrd-1.img -s nocolor", 0));
	ut_assertok(run_command("efidebug boot dump", 0));
	ut_assertok(run_command("efidebug boot order 0001", 0));
	ut_assertok(run_command("bootefi bootmgr", 0));
	ut_assertok(run_command("load", 0));
	ut_assert_nextline("crc32: 0x181464af");
	ut_assertok(run_command("exit", 0));

	ut_assertok(run_command("efidebug boot add "
		"-B 0002 label-2 host 0:1 initrddump.efi "
		"-I host 0:1 initrd-2.img -s nocolor", 0));
	ut_assertok(run_command("efidebug boot dump", 0));
	ut_assertok(run_command("efidebug boot order 0002", 0));
	ut_assertok(run_command("bootefi bootmgr", 0));
	ut_assertok(run_command("load", 0));
	ut_assert_nextline("crc32: 0x811d3515");
	ut_assertok(run_command("exit", 0));

	ut_assertok(run_command("efidebug boot rm 0001", 0));
	ut_assertok(run_command("efidebug boot rm 0002", 0));

	return 0;
}
CMD_TEST(test_efi_bootmgr_norun, UTF_CONSOLE | UTF_MANUAL);
