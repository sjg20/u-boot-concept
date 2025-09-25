// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for 'part_find' command
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/ofnode.h>
#include <dm/test.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Test 'part_find' command */
static int cmd_test_part_find(struct unit_test_state *uts)
{
	struct udevice *dev;
	ofnode root, node;

	/* Enable the requested mmc node since we need a second bootflow */
	root = oftree_root(oftree_default());
	node = ofnode_find_subnode(root, "mmc5");
	ut_assert(ofnode_valid(node));
	ut_assertok(lists_bind_fdt(gd->dm_root, node, &dev, NULL, false));

	ut_assertok(device_probe(dev));

	ut_assertok(env_set("target_part", NULL));
	ut_assertok(run_command("part_find c12a7328-f81f-11d2-ba4b-00a0c93ec93b", 0));
	ut_assert_console_end();
	ut_asserteq_str("mmc 5:c", env_get("target_part"));

	ut_asserteq(1, run_command("part_find invalid", 0));
	ut_asserteq_str("mmc 5:c", env_get("target_part"));

	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_part_find, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);
