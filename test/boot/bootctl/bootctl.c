// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for bootctl
 *
 * For now this is just samples, showing how the different functions can be
 * tested
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
#include <bootctl/measure.h>
#include <bootctl/oslist.h>
#include <bootctl/state.h>
#include "../bootstd_common.h"

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
	bc_oslist_setup_iter(&iter);
	ut_assertok(bc_oslist_next(dev, &iter, &info));
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

	bc_oslist_setup_iter(&iter);
	ut_assertok(bc_oslist_next(dev, &iter, &info));
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

	/* overwrite */
	ut_assertok(bc_state_write_str(dev, "fred", "def"));
	ut_assertok(bc_state_read_str(dev, "fred", &sval));
	ut_asserteq_str("def", sval);

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
	char long_key[32];	/* avoid using constants from impl */
	struct abuf buf;
	char *data;
	int ch;

	ut_assertok(bootctl_get_dev(UCLASS_BOOTCTL_STATE, &dev));

	/* cannot use NULL as a key or value */
	ut_asserteq(-EINVAL, bc_state_write_bool(dev, NULL, false));
	ut_asserteq(-EINVAL, bc_state_write_str(dev, "key", NULL));

	/* empty key and value */
	ut_asserteq(-EINVAL, bc_state_write_str(dev, "", "val"));
	ut_assertok(bc_state_write_str(dev, "empty", ""));

	/* no spaces allowed in a key */
	ut_asserteq(-EKEYREJECTED, bc_state_write_str(dev, "my key", "val"));

	/* check key characters */
	for (ch = 1; ch < 256; ch++) {
		char key[4] = "key";
		bool ok;

		ok = ch == '_' || (ch >= 'a' && ch <= 'z') ||
			(ch >= '0' && ch <= '9');

		key[1] = ch;
		printf("checking ch %x\n", ch);
		if (ok)
			ut_assertok(bc_state_write_str(dev, key, "val"));
		else
			ut_asserteq(-EKEYREJECTED, bc_state_write_str(dev, key, "val"));
	}

	/* key too long */
	strcpy(long_key, "1234567890123456789012345678901");
	ut_asserteq(-EKEYREJECTED, bc_state_write_str(dev, long_key, "val"));
	long_key[30] = '\0';
	ut_assertok(bc_state_write_str(dev, long_key, "val"));

	/* value too long */
	abuf_init(&buf);
	ut_asserteq(true, abuf_realloc(&buf, 0x1002));
	data = buf.data;
	memset(data, 'x', 0x1001);
	data[0x1001] = '\0';
	ut_asserteq(-E2BIG, bc_state_write_str(dev, "try", data));
	data[0x1000] = '\0';
	ut_assertok(bc_state_write_str(dev, "try", data));
	abuf_uninit(&buf);

	return 0;
}
BOOTCTL_TEST(bootctl_simple_state_limits, UTF_DM | UTF_SCAN_FDT);

/* test integers */
static int bootctl_simple_state_int(struct unit_test_state *uts)
{
	struct udevice *dev;
	long ival;

	ut_assertok(bootctl_get_dev(UCLASS_BOOTCTL_STATE, &dev));

	/* basic integers */
	ut_assertok(bc_state_write_int(dev, "val", 0));
	ut_assertok(bc_state_read_int(dev, "val", &ival));
	ut_asserteq(0, ival);

	ut_assertok(bc_state_write_int(dev, "val", 1));
	ut_assertok(bc_state_read_int(dev, "val", &ival));
	ut_asserteq(1, ival);

	ut_assertok(bc_state_write_int(dev, "val", -1));
	ut_assertok(bc_state_read_int(dev, "val", &ival));
	ut_asserteq(-1, ival);

	/* large ints */
	ut_assertok(bc_state_write_int(dev, "val", 0xffffffffl));
	ut_assertok(bc_state_read_int(dev, "val", &ival));
	ut_asserteq(0xffffffffl, ival);

	ut_assertok(bc_state_write_int(dev, "val", -0xffffffffl));
	ut_assertok(bc_state_read_int(dev, "val", &ival));
	ut_asserteq_64(-0xffffffffl, ival);

	ut_assertok(bc_state_write_int(dev, "val", 0x7fffffffffffffffll));
	ut_assertok(bc_state_read_int(dev, "val", &ival));
	ut_asserteq_64(0x7fffffffffffffffll, ival);

	ut_assertok(bc_state_write_int(dev, "val", -0x7fffffffffffffffll));
	ut_assertok(bc_state_read_int(dev, "val", &ival));
	ut_asserteq_64(-0x7fffffffffffffffll, ival);

	return 0;
}
BOOTCTL_TEST(bootctl_simple_state_int, UTF_DM | UTF_SCAN_FDT);

/* test measurement */
static int bootctl_simple_measure(struct unit_test_state *uts)
{
	struct bootflow_img *img[3];
	struct osinfo osinfo;
	struct bootflow *bflow = &osinfo.bflow;
	const struct measure_info *info;
	struct udevice *dev;
	struct alist result;

	ut_assertok(bootctl_get_dev(UCLASS_BOOTCTL_MEASURE, &dev));

	ut_assertok(bc_measure_start(dev));

	/* set up some data */
	memset(&osinfo, '\0', sizeof(struct osinfo));
	alist_init_struct(&bflow->images, struct bootflow_img);

	/* add a few images */
	img[0] = bootflow_img_add(bflow, "kernel",
				  (enum bootflow_img_t)IH_TYPE_KERNEL, 0,
				  0x100);
	ut_assertnonnull(img);
	img[1] = bootflow_img_add(bflow, "initrd",
				  (enum bootflow_img_t)IH_TYPE_RAMDISK, 0x100,
				  0x200);
	ut_assertnonnull(img);

	/* the fdt is missing so this should fail */
	ut_asserteq(-ENOENT, bc_measure_process(dev, &osinfo, &result));
	if (IS_ENABLED(CONFIG_LOGF_FUNC))
		ut_assert_nextline("      simple_process() Missing image 'flat_dt'");
	else
		ut_assert_nextline("Missing image 'flat_dt'");
	ut_assert_console_end();

	alist_uninit(&result);

	img[2] = bootflow_img_add(bflow, "fdt",
				  (enum bootflow_img_t)IH_TYPE_FLATDT, 0x300,
				  0x30);
	ut_assertok(bc_measure_process(dev, &osinfo, &result));

	/* check the result */
	ut_asserteq(3, result.count);
	info = alist_get(&result, 0, struct measure_info);
	ut_asserteq_ptr(img[0], info[0].img);
	ut_asserteq_ptr(img[1], info[1].img);
	ut_asserteq_ptr(img[2], info[2].img);

	/* TODO: We should also a) read out the TPM log and b) check TPM PCRs */

	ut_assertnonnull(img);

	return 0;
}
BOOTCTL_TEST(bootctl_simple_measure, UTF_DM | UTF_SCAN_FDT | UTF_CONSOLE);
