// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for tkey command
 *
 * Copyright (C) 2025 Canonical Ltd
 */

#include <console.h>
#include <dm.h>
#include <dm/test.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Test 'tkey' command help output */
static int cmd_test_tkey_help(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("tkey", 0));
	ut_assert_nextlinen("tkey - Tillitis TKey security token operations");
	ut_assert_nextline_empty();
	ut_assert_nextlinen("Usage:");
	ut_assert_nextlinen("tkey connect");
	ut_assert_skip_to_linen("tkey wrapkey");

	return 0;
}
CMD_TEST(cmd_test_tkey_help, UTF_DM | UTF_SCAN_FDT | UTF_CONSOLE);

/* Test 'tkey' subcommands with emulator */
static int cmd_test_tkey_sandbox(struct unit_test_state *uts)
{
	struct udevice *dev;

	/* TKey device should be available in sandbox */
	ut_assertok(uclass_first_device_err(UCLASS_TKEY, &dev));

	/* Test info command */
	ut_assertok(run_command("tkey info", 0));
	ut_assert_nextline("Name0: tk1  Name1: mkdf Version: 4");
	ut_assert_nextline("UDI: a0a1a2a3a4a5a6a7");

	/* Test fwmode command - should be in firmware mode initially */
	ut_assertok(run_command("tkey fwmode", 0));
	ut_assert_nextline("firmware mode");

	/* Test signer command */
	ut_assertok(run_command("tkey signer", 0));
	ut_assert_nextlinen("signer binary: ");

	/* Test wrapkey command */
	ut_assertok(run_command("tkey wrapkey testpass", 0));
	ut_assert_nextline("Wrapping Key: f91450f0396768885aeaee7f0cc3305de25f6e50db79e7978a83c08896fcbf0d");

	/* Test getkey command */
	ut_assertok(run_command("tkey getkey testuss", 0));
	ut_assert_nextline("Public Key: 505152535455565758595a5b5c5d5e5f505152535455565758595a5b5c5d5e5f");
	ut_assert_nextline("Disk Key: 228b2f6abf8be05649b2417586150bbf3e1b3f669afa1c6151ddc72957933c21");
	ut_assert_nextline("Verification Hash: a72a46b8f8c7ff0824416ada886f62b6c2808896d71201a32814ab432c7a81cf");

	/* After getkey, device should be in app mode */
	ut_assertok(run_command("tkey fwmode", 0));
	ut_assert_nextline("app mode");

	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_tkey_sandbox, UTF_DM | UTF_SCAN_FDT | UTF_CONSOLE);
