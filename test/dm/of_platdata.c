// SPDX-License-Identifier: GPL-2.0+

#include <common.h>
#include <dm.h>
#include <dm/test.h>
#include <test/test.h>
#include <test/ut.h>

static int dm_test_of_platdata_base(struct unit_test_state *uts)
{
	struct udevice *rtc, *i2c;

	printf("dump:\n");
	dm_dump_all();
	ut_assertok(uclass_first_device_err(UCLASS_RTC, &rtc));
	ut_assertok(uclass_first_device_err(UCLASS_I2C, &i2c));
	ut_asserteq_ptr(i2c, dev_get_parent(rtc));

	return 0;
}
DM_TEST(dm_test_of_platdata_base, UT_TESTF_SCAN_PDATA);
