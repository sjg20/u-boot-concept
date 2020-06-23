/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2020 Google LLC
 */

#ifndef __ASM_ARCH_SPI_H
#define __ASM_ARCH_SPI_H

#include <clk.h>

struct rockchip_spi_priv {
	struct rockchip_spi *regs;
	struct clk clk;
	struct tiny_clk tiny_clk;
	unsigned int max_freq;
	unsigned int mode;
	ulong last_transaction_us;	/* Time of last transaction end */
	unsigned int speed_hz;
	unsigned int last_speed_hz;
	uint input_rate;
};

#endif
