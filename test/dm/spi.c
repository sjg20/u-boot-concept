/*
 * Copyright (C) 2013 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <fdtdec.h>
#include <spi.h>
#include <spi_flash.h>
#include <dm/test.h>
#include <dm/ut.h>
#include <dm/util.h>
#include <asm/state.h>

/* Test that sandbox SPI works correctly */
static int dm_test_spi(struct dm_test_state *dms)
{
	struct spi_slave *slave;
	struct udevice *dev;
	const int busnum = 0, cs = 0, mode = 0;
	const char dout[5] = {0x9f};
	unsigned char din[5];

	ut_assertok(spi_get_bus_and_cs(busnum, cs, 1000000, mode, NULL, 0,
				       &dev, &slave));
	ut_assertok(spi_claim_bus(slave));
	ut_assertok(spi_xfer(slave, 40, dout, din,
			     SPI_XFER_BEGIN | SPI_XFER_END));
	ut_asserteq(0xff, din[0]);
	ut_asserteq(0x20, din[1]);
	ut_asserteq(0x20, din[2]);
	ut_asserteq(0x15, din[3]);
	spi_release_bus(slave);

	/*
	 * Since we are about to destroy all devices, we must tell sandbox
	 * to forget the emulation device
	 */
#ifdef CONFIG_DM_SPI_FLASH
	sandbox_sf_unbind_emul(state_get_current(), busnum, cs);
#endif

	return 0;
}
DM_TEST(dm_test_spi, DM_TESTF_SCAN_PDATA | DM_TESTF_SCAN_FDT);
