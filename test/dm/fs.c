// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the filesystems layer
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <dm.h>
#include <fs.h>
#include <dm/test.h>
#include <test/ut.h>

/* Test basic filesystem access */
static int dm_test_fs_base(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_assertok(uclass_first_device_err(UCLASS_FS, &dev));

	ut_assertok(fs_mount(dev));
	ut_asserteq(-EISCONN, fs_mount(dev));

	ut_assertok(fs_unmount(dev));
	ut_asserteq(-ENOTCONN, fs_unmount(dev));

	return 0;
}
DM_TEST(dm_test_fs_base, UTF_SCAN_FDT);
