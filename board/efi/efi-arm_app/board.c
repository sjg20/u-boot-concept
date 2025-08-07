// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2015 Google, Inc
 */

#include <bootm.h>
#include <efi.h>
#include <event.h>
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

int board_exit_boot_services(void *ctx, struct event *evt)
{
	struct efi_priv *priv = efi_get_priv();
	int ret;

	ret = efi_store_memory_map(priv);

	printf("preboot\n");
}
EVENT_SPY_FULL(EVT_BOOTM_FINAL, board_exit_boot_services);
