// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the filesystems layer
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <dir.h>
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

/* Test accessing a directory */
static int dm_test_fs_dir(struct unit_test_state *uts)
{
	struct udevice *fsdev, *dir;
	struct fs_dir_stream *strm;
	struct fs_dirent dent;
	int found;

	ut_assertok(uclass_first_device_err(UCLASS_FS, &fsdev));

	ut_assertok(fs_mount(fsdev));

	ut_assertok(fs_lookup_dir(fsdev, ".", &dir));
	ut_assertnonnull(dir);
	ut_asserteq_str("fs.dir", dir->name);

	ut_assertok(dir_open(dir, &strm));
	found = 0;
	do {
		ut_assertok(dir_read(dir, strm, &dent));
		if (!strcmp("README", dent.name)) {
			ut_asserteq(FS_DT_REG, dent.type);
			found += 1;
		} else if (!strcmp("common", dent.name)) {
			ut_asserteq(FS_DT_DIR, dent.type);
			found += 1;
		}
	} while (found < 2);
	ut_assertok(dir_close(dir, strm));

	ut_assertok(fs_unmount(fsdev));

	return 0;
}
DM_TEST(dm_test_fs_dir, UTF_SCAN_FDT);
