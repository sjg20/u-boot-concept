// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for VBE A/B boot of OS
 *
 * Copyright 2025 Simon Glass <simon.glass@canonical.com>
 */

#include <bootflow.h>
#include <bootstd.h>
#include <dm.h>
#include <mapmem.h>
#include <vbe.h>
#include <test/test.h>
#include <test/ut.h>
#include "bootstd_common.h"
#include "../boot/vbe_abrec.h"

/**
 * check_abrec_norun() - Check operation with or without an OEM FIT
 *
 * @uts: Unit-test state
 * @use_oem: Use an OEM devicetree
 */
static int check_abrec_norun(struct unit_test_state *uts, bool use_oem,
			     enum vbe_pick_t expect_pick)
{
	static const char *order[] = {"host", NULL};
	const struct bootflow_img *img;
	struct bootflow_iter iter;
	struct bootstd_priv *std;
	struct abrec_priv *abpriv;
	struct udevice *bootstd;
	const char **old_order;
	struct bootflow bflow;
	struct vbe_bflow_priv *priv;
	struct udevice *dev;
	ofnode root;
	oftree tree;
	int ret;

	ut_assertok(uclass_first_device_err(UCLASS_BOOTSTD, &bootstd));
	std = dev_get_priv(bootstd);
	old_order = std->bootdev_order;
	std->bootdev_order = order;
	ut_assertok(env_set("boot_targets", NULL));

	ut_assertok(uclass_get_device_by_driver(UCLASS_BOOTMETH,
						DM_DRIVER_GET(vbe_abrec_os),
						&dev));
	abpriv = dev_get_priv(dev);
	abpriv->oem_devicetree = use_oem;

	ret = bootflow_scan_first(NULL, NULL, &iter, BOOTFLOWIF_SHOW, &bflow);
	std->bootdev_order = old_order;
	ut_assertok(ret);

	ut_asserteq_str("host-0.bootdev.part_2", bflow.name);

	/* Check that we got the state OK */
	img = bootflow_img_find(&bflow, BFI_VBE_STATE);
	ut_assertnonnull(img);
	ut_assert(img->addr);

	tree = oftree_from_fdt(map_sysmem(img->addr, 0));
	ut_assert(oftree_valid(tree));

	root = oftree_root(tree);
	ut_asserteq_str("vbe,abrec-state",
			ofnode_read_string(root, "compatible"));

	/* check the private data */
	priv = bflow.bootmeth_priv;
	ut_assertnonnull(priv);
	ut_asserteq(expect_pick, priv->pick_slot);

	if (use_oem) {
		/* Check that we got the OEM FIT */
		img = bootflow_img_find(&bflow, BFI_VBE_OEM_FIT);
		ut_assertnonnull(img);
		ut_assert(img->addr);
		ut_assert(img->size > 0);
		ut_assertok(fit_check_format(map_sysmem(img->addr, img->size),
					     img->size));
	}

	/* Select the first kernel from the extlinux menu */
	ut_asserteq(2, console_in_puts("1\n"));

	/*
	 * We expect it to get through to boot although sandbox always returns
	 * -EFAULT as it cannot actually boot the kernel
	 */
	ut_asserteq(-EFAULT, bootflow_boot(&bflow));

	ut_assert_skip_to_line("VBE: Picked slot %s",
			       priv->pick_slot == VBEP_A ? "a" : "b");

	if (use_oem)
		ut_assert_skip_to_line("Loading OEM devicetree from FIT");

	if (use_oem)
		ut_assert_skip_to_line("Loading OS FIT keeping existing FDT");
	else
		ut_assert_skip_to_line("Loading OS FIT");

	ut_assert_skip_to_line("sandbox: continuing, as we cannot run Linux");
	ut_assert_console_end();

	/*
	 * check the FDT we booted with: we should have loaded conf-1 as the
	 * compatible string for sandbox does not match
	 */
	ut_assertnonnull(working_fdt);
	tree = oftree_from_fdt(working_fdt);
	root = oftree_root(tree);

	ut_asserteq_str("snow", ofnode_read_string(root, "compatible"));

	return 0;
}

/* Test without an OEM FIT */
static int vbe_test_abrec_no_oem_norun(struct unit_test_state *uts)
{
	ut_assertok(check_abrec_norun(uts, false, VBEP_A));

	return 0;
}
BOOTSTD_TEST(vbe_test_abrec_no_oem_norun, UTF_MANUAL | UTF_CONSOLE);

/* Test with an OEM FIT */
static int vbe_test_abrec_oem_norun(struct unit_test_state *uts)
{
	ut_assertok(check_abrec_norun(uts, true, VBEP_B));

	return 0;
}
BOOTSTD_TEST(vbe_test_abrec_oem_norun, UTF_MANUAL | UTF_CONSOLE);
