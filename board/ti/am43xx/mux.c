/*
 * mux.c
 *
 * Copyright (C) 2013 Texas Instruments Incorporated - http://www.ti.com/
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/mux.h>
#include "board.h"

static struct module_pin_mux uart0_pin_mux[] = {
	{OFFSET(uart0_rxd),
	 (MODE(0) | PULLUP_EN | RXACTIVE | SLEWCTRL | DSPULLUDEN)},
	{OFFSET(uart0_txd),
	 (MODE(0) | PULLUDDIS | PULLUP_EN | SLEWCTRL | DSPULLUDEN)},
	{-1},
};

static struct module_pin_mux mmc0_pin_mux[] = {
	{OFFSET(mmc0_clk), (MODE(0) | PULLUDDIS | RXACTIVE | DSPULLUDEN)},
	{OFFSET(mmc0_cmd), (MODE(0) | PULLUP_EN | RXACTIVE | DSPULLUDEN)},
	{OFFSET(mmc0_dat0), (MODE(0) | PULLUP_EN | RXACTIVE | DSPULLUDEN)},
	{OFFSET(mmc0_dat1), (MODE(0) | PULLUP_EN | RXACTIVE | DSPULLUDEN)},
	{OFFSET(mmc0_dat2), (MODE(0) | PULLUP_EN | RXACTIVE | DSPULLUDEN)},
	{OFFSET(mmc0_dat3), (MODE(0) | PULLUP_EN | RXACTIVE | DSPULLUDEN)},
	{-1},
};

static struct module_pin_mux mmc1_pin_mux[] = {
	{OFFSET(gpmc_csn1), (MODE(2) | PULLUDDIS | RXACTIVE | DSPULLUDEN)},
	{OFFSET(gpmc_csn2), (MODE(2) | PULLUP_EN | RXACTIVE | DSPULLUDEN)},
	{OFFSET(gpmc_ad8), (MODE(2) | PULLUP_EN | RXACTIVE | DSPULLUDEN)},
	{OFFSET(gpmc_ad9), (MODE(2) | PULLUP_EN | RXACTIVE | DSPULLUDEN)},
	{OFFSET(gpmc_ad10), (MODE(2) | PULLUP_EN | RXACTIVE | DSPULLUDEN)},
	{OFFSET(gpmc_ad11), (MODE(2) | PULLUP_EN | RXACTIVE | DSPULLUDEN)},
	{-1},
};

static struct module_pin_mux i2c0_pin_mux[] = {
	{OFFSET(i2c0_sda), (MODE(0) | PULLUP_EN | RXACTIVE | SLEWCTRL)},
	{OFFSET(i2c0_scl), (MODE(0) | PULLUP_EN | RXACTIVE | SLEWCTRL)},
	{-1},
};

void enable_uart0_pin_mux(void)
{
	configure_module_pin_mux(uart0_pin_mux);
}

void enable_board_pin_mux(void)
{
	configure_module_pin_mux(mmc0_pin_mux);
	configure_module_pin_mux(mmc1_pin_mux);
	configure_module_pin_mux(i2c0_pin_mux);
}
