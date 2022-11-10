// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for bootdev functions. All start with 'bootdev'
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootstd.h>
#include <dm.h>
#include <bootdev.h>
#include <bootflow.h>
#include <mapmem.h>
#include <os.h>
#include <test/suites.h>
#include <test/ut.h>
#include "bootstd_common.h"

/* Allow reseting the USB-started flag */
extern char usb_started;

/* Check 'bootdev list' command */
static int bootdev_test_cmd_list(struct unit_test_state *uts)
{
	int probed;

	console_record_reset_enable();
	for (probed = 0; probed < 2; probed++) {
		int probe_ch = probed ? '+' : ' ';

		ut_assertok(run_command(probed ? "bootdev list -p" :
			"bootdev list", 0));
		ut_assert_nextline("Seq  Probed  Status  Uclass    Name");
		ut_assert_nextlinen("---");
		ut_assert_nextline("%3x   [ %c ]  %6s  %-8s  %s", 0, probe_ch, "OK",
				   "mmc", "mmc2.bootdev");
		ut_assert_nextline("%3x   [ %c ]  %6s  %-8s  %s", 1, probe_ch, "OK",
				   "mmc", "mmc1.bootdev");
		ut_assert_nextline("%3x   [ %c ]  %6s  %-8s  %s", 2, probe_ch, "OK",
				   "mmc", "mmc0.bootdev");
		ut_assert_nextlinen("---");
		ut_assert_nextline("(3 bootdevs)");
		ut_assert_console_end();
	}

	return 0;
}
BOOTSTD_TEST(bootdev_test_cmd_list, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootdev select' and 'info' commands */
static int bootdev_test_cmd_select(struct unit_test_state *uts)
{
	struct bootstd_priv *std;

	/* get access to the CLI's cur_bootdev */
	ut_assertok(bootstd_get_priv(&std));

	console_record_reset_enable();
	ut_asserteq(1, run_command("bootdev info", 0));
	ut_assert_nextlinen("Please use");
	ut_assert_console_end();

	/* select by sequence */
	ut_assertok(run_command("bootdev select 0", 0));
	ut_assert_console_end();

	ut_assertok(run_command("bootdev info", 0));
	ut_assert_nextline("Name:      mmc2.bootdev");
	ut_assert_nextline("Sequence:  0");
	ut_assert_nextline("Status:    Probed");
	ut_assert_nextline("Uclass:    mmc");
	ut_assert_nextline("Bootflows: 0 (0 valid)");
	ut_assert_console_end();

	/* select by bootdev name */
	ut_assertok(run_command("bootdev select mmc1.bootdev", 0));
	ut_assert_console_end();
	ut_assertnonnull(std->cur_bootdev);
	ut_asserteq_str("mmc1.bootdev", std->cur_bootdev->name);

	/* select by bootdev label*/
	ut_assertok(run_command("bootdev select mmc1", 0));
	ut_assert_console_end();
	ut_assertnonnull(std->cur_bootdev);
	ut_asserteq_str("mmc1.bootdev", std->cur_bootdev->name);

	/* deselect */
	ut_assertok(run_command("bootdev select", 0));
	ut_assert_console_end();
	ut_assertnull(std->cur_bootdev);

	ut_asserteq(1, run_command("bootdev info", 0));
	ut_assert_nextlinen("Please use");
	ut_assert_console_end();

	return 0;
}
BOOTSTD_TEST(bootdev_test_cmd_select, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check bootdev labels */
static int bootdev_test_labels(struct unit_test_state *uts)
{
	struct udevice *dev, *media;
	int mflags = 0;

	ut_assertok(bootdev_find_by_label("mmc2", &dev, &mflags));
	ut_asserteq(UCLASS_BOOTDEV, device_get_uclass_id(dev));
	ut_asserteq(0, mflags);
	media = dev_get_parent(dev);
	ut_asserteq(UCLASS_MMC, device_get_uclass_id(media));
	ut_asserteq_str("mmc2", media->name);

	/* Check method flags */
	ut_assertok(bootdev_find_by_label("pxe", &dev, &mflags));
	ut_asserteq(BOOTFLOW_METHF_PXE_ONLY, mflags);
	ut_assertok(bootdev_find_by_label("dhcp", &dev, &mflags));
	ut_asserteq(BOOTFLOW_METHF_DHCP_ONLY, mflags);

	/* Check invalid uclass */
	ut_asserteq(-EINVAL, bootdev_find_by_label("fred0", &dev, &mflags));

	/* Check unknown sequence number */
	ut_asserteq(-ENOENT, bootdev_find_by_label("mmc6", &dev, &mflags));

	return 0;
}
BOOTSTD_TEST(bootdev_test_labels, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check bootdev_find_by_any() */
static int bootdev_test_any(struct unit_test_state *uts)
{
	struct udevice *dev, *media;
	int mflags;

	/* this happens to have a dev_seq() of 8 ('dm uclass' to see) */
	console_record_reset_enable();
	ut_assertok(bootdev_find_by_any("8", &dev, &mflags));
	ut_asserteq(UCLASS_BOOTDEV, device_get_uclass_id(dev));
	ut_asserteq(0, mflags);
	media = dev_get_parent(dev);
	ut_asserteq(UCLASS_MMC, device_get_uclass_id(media));
	ut_asserteq_str("mmc2", media->name);
	ut_assert_console_end();

	/* there should not be this many bootdevs */
	ut_asserteq(-ENODEV, bootdev_find_by_any("50", &dev, &mflags));
	ut_assert_nextline("Cannot find '50' (err=-19)");
	ut_assert_console_end();

	/* Check method flags */
	ut_assertok(bootdev_find_by_any("pxe", &dev, &mflags));
	ut_asserteq(BOOTFLOW_METHF_PXE_ONLY, mflags);

	/* Check invalid uclass */
	ut_asserteq(-EINVAL, bootdev_find_by_any("fred0", &dev, &mflags));
	ut_assert_nextline("Unknown uclass 'fred0' in label");
	ut_assert_nextline("Cannot find bootdev 'fred0' (err=-22)");
	ut_assert_console_end();

	return 0;
}
BOOTSTD_TEST(bootdev_test_any, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check bootdev ordering with the bootdev-order property */
static int bootdev_test_order(struct unit_test_state *uts)
{
	struct bootflow_iter iter;
	struct bootflow bflow;

	/*
	 * First try the order set by the bootdev-order property
	 * Like all sandbox unit tests this relies on the devicetree setting up
	 * the required devices:
	 *
	 * mmc0 - nothing connected
	 * mmc1 - connected to mmc1.img file
	 * mmc2 - nothing connected
	 */
	ut_assertok(env_set("boot_targets", NULL));
	ut_assertok(bootflow_scan_first(&iter, 0, &bflow));
	ut_asserteq(2, iter.num_devs);
	ut_asserteq_str("mmc2.bootdev", iter.dev_used[0]->name);
	ut_asserteq_str("mmc1.bootdev", iter.dev_used[1]->name);
	bootflow_iter_uninit(&iter);

	/* Use the environment variable to override it */
	ut_assertok(env_set("boot_targets", "mmc1 mmc2"));
	ut_assertok(bootflow_scan_first(&iter, 0, &bflow));
	ut_asserteq(-ENODEV, bootflow_scan_next(&iter, &bflow));
	ut_asserteq(2, iter.num_devs);
	ut_asserteq_str("mmc1.bootdev", iter.dev_used[0]->name);
	ut_asserteq_str("mmc2.bootdev", iter.dev_used[1]->name);
	bootflow_iter_uninit(&iter);

	return 0;
}
BOOTSTD_TEST(bootdev_test_order, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check default bootdev ordering  */
static int bootdev_test_order_default(struct unit_test_state *uts)
{
	struct bootflow_iter iter;
	struct bootflow bflow;

	/*
	 * Now drop both orderings, to check the default (prioriy/sequence)
	 * ordering
	 */
	ut_assertok(env_set("boot_targets", NULL));
	ut_assertok(bootstd_test_drop_bootdev_order(uts));

	ut_assertok(bootflow_scan_first(&iter, 0, &bflow));
	ut_asserteq(2, iter.num_devs);
	ut_asserteq_str("mmc2.bootdev", iter.dev_used[0]->name);
	ut_asserteq_str("mmc1.bootdev", iter.dev_used[1]->name);

	ut_asserteq(-ENODEV, bootflow_scan_next(&iter, &bflow));
	ut_asserteq(3, iter.num_devs);
	ut_asserteq_str("mmc0.bootdev", iter.dev_used[2]->name);
#if 0
	/*
	 * Check that adding aliases for the bootdevs works. We just fake it by
	 * setting the sequence numbers directly.
	 */
	iter.dev_order[0]->seq_ = 0;
	iter.dev_order[1]->seq_ = 3;
	iter.dev_order[2]->seq_ = 2;
	bootflow_iter_uninit(&iter);

	ut_assertok(bootflow_scan_first(&iter, 0, &bflow));
	ut_asserteq(3, iter.num_devs);
	ut_asserteq_str("mmc2.bootdev", iter.dev_order[0]->name);
	ut_asserteq_str("mmc0.bootdev", iter.dev_order[1]->name);
	ut_asserteq_str("mmc1.bootdev", iter.dev_order[2]->name);
#endif
	bootflow_iter_uninit(&iter);

	return 0;
}
BOOTSTD_TEST(bootdev_test_order_default, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check bootdev ordering with the uclass priority */
static int bootdev_test_prio(struct unit_test_state *uts)
{
	struct bootdev_uc_plat *ucp;
	struct bootflow_iter iter;
	struct bootflow bflow;
	struct udevice *blk;

	state_set_skip_delays(true);

	/* Start up USB which gives us three additional bootdevs */
	usb_started = false;
	ut_assertok(run_command("usb start", 0));

	ut_assertok(bootstd_test_drop_bootdev_order(uts));

	/* 3 MMC and 3 USB bootdevs: MMC should come before USB */
	console_record_reset_enable();
	ut_assertok(bootflow_scan_first(&iter, 0, &bflow));
	ut_asserteq(-ENODEV, bootflow_scan_next(&iter, &bflow));
	ut_asserteq(6, iter.num_devs);
	ut_asserteq_str("mmc2.bootdev", iter.dev_used[0]->name);
	ut_asserteq_str("usb_mass_storage.lun0.bootdev",
			iter.dev_used[3]->name);

	ut_assertok(bootdev_get_sibling_blk(iter.dev_used[3], &blk));
	ut_asserteq_str("usb_mass_storage.lun0", blk->name);

	/* adjust the priority of the first USB bootdev to the highest */
	ucp = dev_get_uclass_plat(iter.dev_used[3]);
	ucp->prio = BOOTDEVP_1_PRE_SCAN;

	printf("\n\nstart\n");
	bootflow_iter_uninit(&iter);
	ut_assertok(bootflow_scan_first(&iter, BOOTFLOWF_HUNT, &bflow));
	ut_asserteq(-ENODEV, bootflow_scan_next(&iter, &bflow));
	ut_asserteq(6, iter.num_devs);
	ut_asserteq_str("usb_mass_storage.lun0.bootdev",
			iter.dev_used[0]->name);
	ut_asserteq_str("mmc2.bootdev", iter.dev_used[1]->name);

	return 0;
}
BOOTSTD_TEST(bootdev_test_prio, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check 'bootdev hunt' command */
static int bootdev_test_cmd_hunt(struct unit_test_state *uts)
{
	struct bootstd_priv *std;

	/* get access to the used hunters */
	ut_assertok(bootstd_get_priv(&std));

	console_record_reset_enable();
	ut_assertok(run_command("bootdev hunt -l", 0));
	ut_assert_nextline("Prio  Used  Uclass           Hunter");
	ut_assert_nextlinen("----");
	ut_assert_nextline("   6        ethernet         eth_bootdev");
	ut_assert_skip_to_line("(total hunters: 8)");
	ut_assert_console_end();

	/* Use the MMC hunter and see that it updates */
	ut_assertok(run_command("bootdev hunt mmc", 0));
	ut_assertok(run_command("bootdev hunt -l", 0));
	ut_assert_skip_to_line("   5        ide              ide_bootdev");
	ut_assert_nextline("   2     *  mmc              mmc_bootdev");
	ut_assert_skip_to_line("(total hunters: 8)");
	ut_assert_console_end();

	/* Scan all hunters */
	sandbox_set_eth_enable(false);
	state_set_skip_delays(true);
	ut_assertok(run_command("bootdev hunt", 0));
	ut_assertok(run_command("bootdev hunt -l", 0));
	ut_asserteq(GENMASK(7, 0), std->hunters_used);

	return 0;
}
BOOTSTD_TEST(bootdev_test_cmd_hunt, UT_TESTF_DM | UT_TESTF_SCAN_FDT);

/* Check searching for bootdevs using the hunters */
static int bootdev_test_hunt_scan(struct unit_test_state *uts)
{
	struct bootflow_iter iter;
	struct bootflow bflow;

	ut_assertok(bootstd_test_drop_bootdev_order(uts));
	ut_assertok(bootflow_scan_first(&iter, BOOTFLOWF_SHOW | BOOTFLOWF_HUNT |
					BOOTFLOWF_SKIP_GLOBAL, &bflow));
	ut_assertok(bootstd_test_check_mmc_hunter(uts));

	return 0;
}
BOOTSTD_TEST(bootdev_test_hunt_scan, UT_TESTF_DM | UT_TESTF_SCAN_FDT);
