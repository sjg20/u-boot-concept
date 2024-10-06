// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <bloblist.h>
#include <bootstage.h>
#include <command.h>
#include <malloc.h>
#include <mapmem.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

static void print_region(const char *name, ulong base, ulong size, ulong *uptop)
{
	ulong end = base + size;

	printf("%-12s %8lx %8lx %8lx", name, base, size, end);
	if (*uptop)
		printf(" %8lx", *uptop - end);
	putc('\n');
	*uptop = base;
}

static int do_memmap(struct cmd_tbl *cmdtp, int flag, int argc,
		     char *const argv[])
{
	ulong upto, stk_bot;

	printf("%-12s %8s %8s %8s %8s\n", "Region", "Base", "Size", "End",
	       "Gap");
	printf("------------------------------------------------\n");
	upto = 0;
	if (IS_ENABLED(CONFIG_VIDEO))
		print_region("video", gd_video_bottom(),
			     gd_video_size(), &upto);
	if (IS_ENABLED(CONFIG_TRACE))
		print_region("trace", map_to_sysmem(gd_trace_buff()),
			     gd_trace_size(), &upto);
	print_region("code", gd->relocaddr, gd->mon_len, &upto);
	print_region("malloc", mem_malloc_start,
		     mem_malloc_end - mem_malloc_start, &upto);
	print_region("board_info", map_to_sysmem(gd->bd),
		     sizeof(struct bd_info), &upto);
	print_region("global_data", map_to_sysmem(gd),
		     sizeof(struct global_data), &upto);
	print_region("devicetree", map_to_sysmem(gd->fdt_blob),
		     fdt_totalsize(gd->fdt_blob), &upto);
	if (IS_ENABLED(CONFIG_BOOTSTAGE))
		print_region("bootstage", map_to_sysmem(gd_bootstage()),
			     bootstage_get_size(false), &upto);
	if (IS_ENABLED(CONFIG_BLOBLIST))
		print_region("bloblist", map_to_sysmem(gd_bloblist()),
			     bloblist_get_total_size(), &upto);
	stk_bot = gd->start_addr_sp - CONFIG_STACK_SIZE;
	print_region("stack", stk_bot, CONFIG_STACK_SIZE, &upto);
	print_region("free", gd->ram_base, stk_bot, &upto);

	return 0;
}

U_BOOT_CMD(
	memmap,	1, 1, do_memmap, "Show a map of U-Boot's memory usage", ""
);
