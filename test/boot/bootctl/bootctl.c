// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for bootctl
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#include <stdbool.h>
#include <bootctl.h>
#include <bootmeth.h>
#include <dm.h>
#include <os.h>
#include <test/ut.h>
#include "bootctl_common.h"
#include "../../../boot/bootctl/oslist.h"
#include "../bootstd_common.h"
#include "../../../boot/bootctl/state.h"

/* test that expected devices are available and can be probed */
static int bootctl_base(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_assertok(bootctl_get_dev(UCLASS_BOOTCTL_UI, &dev));
	ut_asserteq_str("ui", dev->name);

	ut_assertok(bootctl_get_dev(UCLASS_BOOTCTL_OSLIST, &dev));
	ut_asserteq_str("oslist", dev->name);

	ut_assertok(bootctl_get_dev(UCLASS_BOOTCTL_STATE, &dev));
	ut_asserteq_str("state", dev->name);

	return 0;
}
BOOTCTL_TEST(bootctl_base, UTF_DM | UTF_SCAN_FDT);

/* test finding an OS */
static int bootctl_oslist(struct unit_test_state *uts)
{
	struct oslist_iter iter;
	struct osinfo info;
	struct bootflow *bflow = &info.bflow;
	struct udevice *dev;

	ut_assertok(bootctl_get_dev(UCLASS_BOOTCTL_OSLIST, &dev));
	ut_asserteq_str("oslist", dev->name);

	/* initially we should only see Fedora */
	ut_assertok(bc_oslist_first(dev, &iter, &info));
	ut_asserteq_str("mmc1.bootdev.part_1", bflow->name);
	ut_asserteq_strn("Fedora-Workstation", bflow->os_name);

	ut_asserteq(-ENODEV, bc_oslist_next(dev, &iter, &info));

	return 0;
}
BOOTCTL_TEST(bootctl_oslist, UTF_DM | UTF_SCAN_FDT);

/* test finding OSes on mmc and usb */
static int bootctl_oslist_usb(struct unit_test_state *uts)
{
	struct oslist_iter iter;
	struct osinfo info;
	struct bootflow *bflow = &info.bflow;
	struct udevice *dev;

	test_set_skip_delays(true);
	bootstd_reset_usb();

	ut_assertok(bootctl_get_dev(UCLASS_BOOTCTL_OSLIST, &dev));
	ut_asserteq_str("oslist", dev->name);

	/* include usb in the bootdev order */
	ut_assertok(bootdev_set_order("mmc usb"));

	ut_assertok(bc_oslist_first(dev, &iter, &info));
	ut_asserteq_str("mmc1.bootdev.part_1", bflow->name);

	ut_assertok(bc_oslist_next(dev, &iter, &info));
	ut_asserteq_str("hub1.p4.usb_mass_storage.lun0.bootdev.part_1", bflow->name);

	ut_asserteq(-ENODEV, bc_oslist_next(dev, &iter, &info));

	return 0;
}
BOOTCTL_TEST(bootctl_oslist_usb, UTF_DM | UTF_SCAN_FDT);

/* test basic use of state */
static int bootctl_simple_state_base(struct unit_test_state *uts)
{
	struct udevice *dev;
	const char *sval;
	struct abuf buf;
	bool bval;
	long ival;

	ut_assertok(bootctl_get_dev(UCLASS_BOOTCTL_STATE, &dev));
	ut_assertok(bc_state_write_bool(dev, "fred", false));
	ut_assertok(bc_state_write_bool(dev, "mary", true));
	ut_assertok(bc_state_write_int(dev, "alex", 123));
	ut_assertok(bc_state_write_str(dev, "john", "abc"));

	ut_assertok(bc_state_read_bool(dev, "fred", &bval));
	ut_asserteq(false, bval);

	ut_assertok(bc_state_read_bool(dev, "mary", &bval));
	ut_asserteq(true, bval);

	ut_assertok(bc_state_read_int(dev, "alex", &ival));
	ut_asserteq(123, ival);

	ut_assertok(bc_state_read_str(dev, "john", &sval));
	ut_asserteq_str("abc", sval);

	/* check the buffer contents, including the nul terminator */
	ut_assertok(bc_state_save_to_buf(dev, &buf));
	ut_asserteq_str("fred=0\nmary=1\nalex=123\njohn=abc\n", buf.data);
	ut_asserteq(strlen("fred=0\nmary=1\nalex=123\njohn=abc\n") + 1,
		    buf.size);
	ut_asserteq(0, *((char *)buf.data + buf.size - 1));
	abuf_uninit(&buf);

	ut_assertok(bc_state_clear(dev));
	ut_asserteq(-ENOENT, bc_state_read_bool(dev, "fred", &bval));
	ut_asserteq(-ENOENT, bc_state_read_bool(dev, "mary", &bval));
	ut_asserteq(-ENOENT, bc_state_read_bool(dev, "john", &bval));
	ut_asserteq(-ENOENT, bc_state_read_bool(dev, "alex", &bval));

	return 0;
}
BOOTCTL_TEST(bootctl_simple_state_base, UTF_DM | UTF_SCAN_FDT);

/* test loading / saving state */
static int bootctl_simple_state_loadsave(struct unit_test_state *uts)
{
	struct udevice *dev;
	char *buf;
	int size;

	ut_assertok(bootctl_get_dev(UCLASS_BOOTCTL_STATE, &dev));
	ut_assertok(bc_state_write_bool(dev, "fred", false));
	ut_assertok(bc_state_write_bool(dev, "mary", true));
	ut_assertok(bc_state_save(dev));

	/* check the file contents, including the nul terminator */
	ut_assertok(os_read_file("bootctl.ini", (void **)&buf, &size));
	ut_asserteq_str("fred=0\nmary=1\n", buf);
	ut_asserteq(strlen("fred=0\nmary=1\n") + 1, size);
	ut_asserteq(0, buf[size - 1]);
	os_free(buf);

	ut_assertok(bc_state_load(dev));

	return 0;
}
BOOTCTL_TEST(bootctl_simple_state_loadsave, UTF_DM | UTF_SCAN_FDT);

/* test limits */
static int bootctl_simple_state_limits(struct unit_test_state *uts)
{
	struct udevice *dev;
	struct abuf buf;

	ut_assertok(bootctl_get_dev(UCLASS_BOOTCTL_STATE, &dev));
	ut_asserteq(-EINVAL, bc_state_write_bool(dev, NULL, false));
	ut_asserteq(-EINVAL, bc_state_write_str(dev, "key", NULL));

	/*
	 * other things to add here:
	 * - keys and string values too long
	 * - negative integers and the limits of integer range
	 */

	return 0;
}
BOOTCTL_TEST(bootctl_simple_state_limits, UTF_DM | UTF_SCAN_FDT);

