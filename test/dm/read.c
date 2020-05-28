// SPDX-License-Identifier: GPL-2.0+
/*
 * Livetree API
 *
 * Copyright 2020 Google LLC
 */

#include <common.h>
#include <dm.h>
#include <mapmem.h>
#include <syscon.h>
#include <asm/test.h>
#include <dm/test.h>
#include <test/ut.h>

/* Test that dev_read_addr_ptr() works in flattree and livetree */
static int dm_test_dev_read_addr_ptr(struct unit_test_state *uts)
{
	struct udevice *gpio, *dev;
	void *ptr;

	/* Test for missing reg property */
	ut_assertok(uclass_first_device_err(UCLASS_GPIO, &gpio));
	ut_assertnull(dev_read_addr_ptr(gpio));

	ut_assertok(syscon_get_by_driver_data(SYSCON0, &dev));
	ptr = dev_read_addr_ptr(dev);
	ut_asserteq(0x10, map_to_sysmem(ptr));

	/* See dm_test_fdt_translation() which has more tests */

	return 0;
}
DM_TEST(dm_test_dev_read_addr_ptr, DM_TESTF_SCAN_PDATA | DM_TESTF_SCAN_FDT);
