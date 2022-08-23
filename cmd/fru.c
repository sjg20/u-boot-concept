// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2019 - 2020 Xilinx, Inc.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <common.h>
#include <command.h>
#include <fdtdec.h>
#include <fru.h>
#include <malloc.h>
#include <mapmem.h>

static int do_fru_capture(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	unsigned long addr;
	const void *buf;
	char *endp;
	int ret;

	if (argc < cmdtp->maxargs)
		return CMD_RET_USAGE;

	addr = hextoul(argv[2], &endp);
	if (*argv[1] == 0 || *endp != 0)
		return -1;

	buf = map_sysmem(addr, 0);
	ret = fru_capture(buf);
	unmap_sysmem(buf);

	return ret;
}

static int do_fru_display(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	fru_display(1);
	return CMD_RET_SUCCESS;
}

static int do_fru_generate(struct cmd_tbl *cmdtp, int flag, int argc,
			   char *const argv[])
{
	int (*fru_generate)(const void *addr, int argc, char *const argv[]);
	unsigned long addr;
	const void *buf;
	int ret, maxargs;

	if (!strncmp(argv[2], "-b", 3)) {
		fru_generate = fru_board_generate;
		maxargs = cmdtp->maxargs + FRU_BOARD_AREA_TOTAL_FIELDS;
	} else if (!strncmp(argv[2], "-p", 3)) {
		fru_generate = fru_product_generate;
		maxargs = cmdtp->maxargs + FRU_PRODUCT_AREA_TOTAL_FIELDS;
	} else {
		return CMD_RET_USAGE;
	}

	if (argc < maxargs)
		return CMD_RET_USAGE;

	addr = hextoul(argv[3], NULL);

	buf = map_sysmem(addr, 0);
	ret = fru_generate(buf, argc - 3, argv + 3);
	unmap_sysmem(buf);

	return ret;
}

static struct cmd_tbl cmd_fru_sub[] = {
	U_BOOT_CMD_MKENT(capture, 3, 0, do_fru_capture, "", ""),
	U_BOOT_CMD_MKENT(display, 2, 0, do_fru_display, "", ""),
	U_BOOT_CMD_MKENT(generate, 4, 0, do_fru_generate, "", ""),
};

static int do_fru(struct cmd_tbl *cmdtp, int flag, int argc,
		  char *const argv[])
{
	struct cmd_tbl *c;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	c = find_cmd_tbl(argv[1], &cmd_fru_sub[0],
			 ARRAY_SIZE(cmd_fru_sub));
	if (!c)
		return CMD_RET_USAGE;

	ret = c->cmd(c, flag, argc, argv);

	return cmd_process_error(c, ret);
}

/***************************************************/
#ifdef CONFIG_SYS_LONGHELP
static char fru_help_text[] =
	"capture <addr> - Parse and capture FRU table present at address.\n"
	"fru display - Displays content of FRU table that was captured using\n"
	"              fru capture command\n"
	"fru generate -b <addr> <manufacturer> <board name> <serial number>\n"
	"                <part number> <file id> [custom ...] - Generate FRU\n"
	"                format with board info area filled based on\n"
	"                parameters. <addr> is pointing to place where FRU is\n"
	"                generated.\n"
	"fru generate -p <addr> <manufacturer> <product name> <part number>\n"
	"                <version number> <serial number> <asset number>\n"
	"                <file id> [custom ...] - Generate FRU format with\n"
	"                product info area filled based on parameters. <addr>\n"
	"                is pointing to place where FRU is generated.\n"
	;
#endif

U_BOOT_CMD(
	fru, CONFIG_SYS_MAXARGS, 1, do_fru,
	"FRU table info",
	fru_help_text
)
