// SPDX-License-Identifier: GPL-2.0+
/*
 * Board init file for Qualcomm SDM845 MTP
 *
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>

DECLARE_GLOBAL_DATA_PTR;

int dram_init(void)
{
	gd->ram_size = PHYS_SDRAM_1_SIZE;

	return 0;
}

int dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;

	return 0;
}

int board_init(void)
{
	return 0;
}

void reset_cpu(ulong addr)
{
}

int print_cpuinfo(void)
{
	return 0;
}
