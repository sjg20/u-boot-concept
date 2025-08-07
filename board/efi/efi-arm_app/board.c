// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2015 Google, Inc
 */

#include <bootm.h>
#include <init.h>

struct mm_region *mem_map;

int print_cpuinfo(void)
{
	return 0;
}

int board_init(void)
{
	return 0;
}

void board_preboot_os(void)
{
	printf("preboot\n");
}
