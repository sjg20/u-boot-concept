/*
 * Copyright (C) 2013 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <cros_ec.h>
#include <asm/arch/bd82x6x.h>

int arch_early_init_r(void)
{
// 	if (cros_ec_board_init())
// 		return -1;

	return 0;
}

int board_early_init_r(void)
{
// 	bd82x6x_init();

	return 0;
}
