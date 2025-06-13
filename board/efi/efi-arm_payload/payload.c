// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <init.h>
#include <asm/armv8/mmu.h>

struct mm_region *mem_map;

void reset_cpu(void)
{
}

int dram_init(void)
{
	return 0;
}

int board_init(void)
{
	return 0;
}
