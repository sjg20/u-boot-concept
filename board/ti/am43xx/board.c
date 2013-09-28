/*
 * board.c
 *
 * Board functions for TI AM43XX based boards
 *
 * Copyright (C) 2013, Texas Instruments, Incorporated - http://www.ti.com/
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <spl.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/mux.h>
#include <asm/arch/ddr_defs.h>
#include <asm/emif.h>
#include "board.h"

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_SPL_BUILD

const struct dpll_params dpll_ddr = {
		266, 24, 1, -1, 1, -1, -1};

const struct emif_regs emif_regs = {
	//.sdram_config_init		= 0x80800EBA,
	.sdram_config			= 0x808012BA,
	.ref_ctrl			= 0x0000040D,
	.sdram_tim1			= 0xEA86B412,
	.sdram_tim2			= 0x1025094A,
	.sdram_tim3			= 0x0F6BA22F,
	.read_idle_ctrl			= 0x00050000,
	.zq_config			= 0xD00fFFFF,
	.temp_alert_config		= 0x0,
	//.emif_ddr_phy_ctlr_1_init	= 0x0E28420d,
	.emif_ddr_phy_ctlr_1		= 0x0E084006,
	.emif_ddr_ext_phy_ctrl_1	= 0x04010040,
	.emif_ddr_ext_phy_ctrl_2	= 0x00500050,
	.emif_ddr_ext_phy_ctrl_3	= 0x00500050,
	.emif_ddr_ext_phy_ctrl_4	= 0x00500050,
	.emif_ddr_ext_phy_ctrl_5	= 0x00500050
};

const struct dpll_params *get_dpll_ddr_params(void)
{
	return &dpll_ddr;
}

void set_uart_mux_conf(void)
{
	enable_uart0_pin_mux();
}

void set_mux_conf_regs(void)
{
	enable_board_pin_mux();
}

void sdram_init(void)
{
	do_sdram_init(&emif_regs);
}
#endif

int board_init(void)
{
	gd->bd->bi_boot_params = PHYS_DRAM_1 + 0x100;

	return 0;
}

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
	return 0;
}
#endif
