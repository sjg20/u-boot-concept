/*
 * Copyright (C) 2013 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <fdtdec.h>
#include <i2c.h>
#include <dm/device-internal.h>
#include <dm/test.h>
#include <dm/uclass-internal.h>
#include <dm/ut.h>
#include <dm/util.h>
#include <asm/state.h>
#include <asm/test.h>

static const int busnum;
static const int chip = 0x2c;

/* Test that we can find buses and chips */
static int dm_test_i2c_find(struct dm_test_state *dms)
{
	struct udevice *bus, *dev;
	const int no_chip = 0x10;

	ut_asserteq(-ENODEV, uclass_find_device_by_seq(UCLASS_I2C, busnum,
						       false, &bus));

	/*
	 * i2c_post_bind() will bind devices to chip selects. Check this then
	 * remove the emulation and the slave device.
	 */
	ut_assertok(uclass_get_device_by_seq(UCLASS_I2C, busnum, &bus));
	ut_assertok(i2c_probe(bus, chip, &dev));
	ut_asserteq(-ENODEV, i2c_probe(bus, no_chip, &dev));
	ut_asserteq(-ENODEV, uclass_get_device_by_seq(UCLASS_I2C, 1, &bus));

	return 0;
}
DM_TEST(dm_test_i2c_find, DM_TESTF_SCAN_PDATA | DM_TESTF_SCAN_FDT);

static int dm_test_i2c_read_write(struct dm_test_state *dms)
{
	struct udevice *bus, *dev;
	uint8_t buf[5];

	ut_assertok(uclass_get_device_by_seq(UCLASS_I2C, busnum, &bus));
	ut_assertok(i2c_get_chip(bus, chip, &dev));
	ut_assertok(i2c_read(dev, 0, buf, 5));
	ut_assertok(memcmp(buf, "\0\0\0\0\0", sizeof(buf)));
	ut_assertok(i2c_write(dev, 2, (uint8_t *)"AB", 2));
	ut_assertok(i2c_read(dev, 0, buf, 5));
	ut_assertok(memcmp(buf, "\0\0AB\0", sizeof(buf)));

	return 0;
}
DM_TEST(dm_test_i2c_read_write, DM_TESTF_SCAN_PDATA | DM_TESTF_SCAN_FDT);

static int dm_test_i2c_speed(struct dm_test_state *dms)
{
	struct udevice *bus, *dev;
	uint8_t buf[5];

	ut_assertok(uclass_get_device_by_seq(UCLASS_I2C, busnum, &bus));
	ut_assertok(i2c_get_chip(bus, chip, &dev));
	ut_assertok(i2c_set_bus_speed(bus, 100000));
	ut_assertok(i2c_read(dev, 0, buf, 5));
	ut_assertok(i2c_set_bus_speed(bus, 400000));
	ut_asserteq(400000, i2c_get_bus_speed(bus));
	ut_assertok(i2c_read(dev, 0, buf, 5));
	ut_asserteq(-EINVAL, i2c_write(dev, 0, buf, 5));

	return 0;
}
DM_TEST(dm_test_i2c_speed, DM_TESTF_SCAN_PDATA | DM_TESTF_SCAN_FDT);

static int dm_test_i2c_addr_len(struct dm_test_state *dms)
{
	struct udevice *bus, *dev;
	uint8_t buf[5];

	ut_assertok(uclass_get_device_by_seq(UCLASS_I2C, busnum, &bus));
	ut_assertok(i2c_get_chip(bus, chip, &dev));
	ut_assertok(i2c_set_addr_len(dev, 1));
	ut_assertok(i2c_read(dev, 0, buf, 5));

	/* The sandbox driver allows this setting, but then fails reads */
	ut_assertok(i2c_set_addr_len(dev, 2));
	ut_asserteq(-EINVAL, i2c_read(dev, 0, buf, 5));

	/* This is not supported by the uclass */
	ut_asserteq(-EINVAL, i2c_set_addr_len(dev, 4));

	/* This is faulted by the sandbox driver */
	ut_asserteq(-EINVAL, i2c_set_addr_len(dev, 3));

	return 0;
}
DM_TEST(dm_test_i2c_addr_len, DM_TESTF_SCAN_PDATA | DM_TESTF_SCAN_FDT);

static int dm_test_i2c_probe_empty(struct dm_test_state *dms)
{
	struct udevice *bus, *dev;

	ut_assertok(uclass_get_device_by_seq(UCLASS_I2C, busnum, &bus));
	ut_assertok(i2c_probe(bus, SANDBOX_I2C_TEST_ADDR, &dev));

	return 0;
}
DM_TEST(dm_test_i2c_probe_empty, DM_TESTF_SCAN_PDATA | DM_TESTF_SCAN_FDT);
