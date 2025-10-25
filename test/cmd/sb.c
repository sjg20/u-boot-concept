// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for sb command
 *
 * Copyright (C) 2025 Canonical Ltd
 */

#include <dm.h>
#include <dm/test.h>
#include <test/test.h>
#include <test/ut.h>

/* Basic test of 'sb devon' and 'sb devoff' commands */
static int dm_test_sb_devon_devoff(struct unit_test_state *uts)
{
	struct udevice *dev;
	ofnode root, node;

	/* Find the mmc11 device tree node */
	root = oftree_root(oftree_default());
	node = ofnode_find_subnode(root, "mmc11");
	ut_assert(ofnode_valid(node));

	/* Verify device is not initially bound */
	ut_assert(device_find_global_by_ofnode(node, &dev));

	/* Enable the device using 'sb devon' */
	ut_assertok(run_command("sb devon mmc11", 0));
	ut_assert_nextline("Device 'mmc11' enabled");
	ut_assert_console_end();

	/* Verify device is now bound and probed */
	ut_assertok(device_find_global_by_ofnode(node, &dev));
	ut_assertnonnull(dev);
	ut_assert(device_active(dev));

	/* Disable the device using 'sb devoff' */
	ut_assertok(run_command("sb devoff mmc11", 0));
	ut_assert_nextline("Device 'mmc11' disabled");
	ut_assert_console_end();

	/* Verify device is no longer bound */
	ut_assert(device_find_global_by_ofnode(node, &dev));

	return 0;
}
DM_TEST(dm_test_sb_devon_devoff, UTF_SCAN_FDT | UTF_CONSOLE);

/* Test 'sb devon' with invalid node */
static int dm_test_sb_devon_invalid(struct unit_test_state *uts)
{
	/* Try to enable non-existent device */
	ut_asserteq(1, run_command("sb devon nonexistent", 0));
	ut_assert_nextline("Device tree node 'nonexistent' not found");
	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_sb_devon_invalid, UTF_SCAN_FDT | UTF_CONSOLE);

/* Test 'sb devoff' with invalid node */
static int dm_test_sb_devoff_invalid(struct unit_test_state *uts)
{
	/* Try to disable non-existent device */
	ut_asserteq(1, run_command("sb devoff nonexistent", 0));
	ut_assert_nextline("Device tree node 'nonexistent' not found");
	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_sb_devoff_invalid, UTF_SCAN_FDT | UTF_CONSOLE);

/* Test 'sb devon' on device that's already enabled */
static int dm_test_sb_devon_already_enabled(struct unit_test_state *uts)
{
	/* Enable the device first */
	ut_assertok(run_command("sb devon mmc11", 0));
	ut_assert_nextline("Device 'mmc11' enabled");
	ut_assert_console_end();

	/* Try to enable it again - should fail */
	ut_asserteq(1, run_command("sb devon mmc11", 0));
	ut_assert_nextline("Device 'mmc11' is already enabled");
	ut_assert_console_end();

	/* Clean up - disable the device */
	ut_assertok(run_command("sb devoff mmc11", 0));
	ut_assert_nextline("Device 'mmc11' disabled");
	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_sb_devon_already_enabled, UTF_SCAN_FDT | UTF_CONSOLE);

/* Test 'sb devoff' on device that's not bound */
static int dm_test_sb_devoff_not_bound(struct unit_test_state *uts)
{
	struct udevice *dev;
	ofnode root, node;

	/* Find the mmc11 device tree node */
	root = oftree_root(oftree_default());
	node = ofnode_find_subnode(root, "mmc11");
	ut_assert(ofnode_valid(node));

	/* Ensure device is not bound (clean up from any previous test) */
	if (!device_find_global_by_ofnode(node, &dev)) {
		ut_assertok(run_command("sb devoff mmc11", 0));
		ut_assert_nextlinen("Device 'mmc11' disabled");
		ut_assert_console_end();
	}

	/* Verify device is not bound */
	ut_assert(device_find_global_by_ofnode(node, &dev));

	/* Try to disable a device that's not bound */
	ut_asserteq(1, run_command("sb devoff mmc11", 0));
	ut_assert_nextlinen("Device 'mmc11' not found or not bound");
	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_sb_devoff_not_bound, UTF_SCAN_FDT | UTF_CONSOLE);
