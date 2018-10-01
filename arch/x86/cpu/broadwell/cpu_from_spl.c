// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2016 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bloblist.h>
#include <debug_uart.h>
#include <handoff.h>
#include <asm/mtrr.h>

int misc_init_r(void)
{
	return 0;
}

int dram_init(void)
{
	int size;
	struct spl_handoff *ho = bloblist_find(BLOBLISTT_SPL_HANDOFF, &size);

	if (!ho)
		return log_msg_ret("Missing SPL hand-off info", -ENOENT);
	handoff_load_dram_size(ho);
// 	mtrr_add_request(MTRR_TYPE_WRBACK, 0, gd->ram_size);
	mtrr_add_request(MTRR_TYPE_WRBACK, 0, 2ULL << 30);
// 	mtrr_commit(true);

	return 0;
}

int checkcpu(void)
{
	return 0;
}

int print_cpuinfo(void)
{
	return 0;
}

void board_debug_uart_init(void)
{
}

int dram_init_banksize(void)
{
#ifdef CONFIG_NR_DRAM_BANKS
	int size;
	struct spl_handoff *ho = bloblist_find(BLOBLISTT_SPL_HANDOFF, &size);

	if (!ho)
		return log_msg_ret("Missing SPL hand-off info", -ENOENT);
	handoff_load_dram_banks(ho);
#endif

	return 0;
}
