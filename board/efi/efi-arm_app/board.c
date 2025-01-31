// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2015 Google, Inc
 */

#include <init.h>

struct mm_region *mem_map;

int dram_init(void)
{
	/* gd->ram_size is set as part of EFI init */

	return 0;
}

void reset_cpu(void)
{
}

int print_cpuinfo(void)
{
	return 0;
}

int board_init(void)
{
	return 0;
}
