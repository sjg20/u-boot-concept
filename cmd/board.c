/*
 * Board driver commands
 *
 * Copyright (c) 2017 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <board.h>
#include <command.h>

DECLARE_GLOBAL_DATA_PTR;

static const char * const phase_name[] = {
	[BOARD_F_ARCH_CPU_INIT_DM] = "arch_cpu_init_dm",
	[BOARD_F_EARLY_INIT_F] = "board_early_init_f",
	[BOARD_F_CHECKCPU] = "checkcpu",
	[BOARD_F_MISC_INIT_F] = "misc_init_f",
	[BOARD_F_DRAM_INIT] = "dram_init",
	[BOARD_F_RESERVE_ARCH] = "reserve_arch",
	[BOARD_PHASE_TEST] = "test",
	[BOARD_PHASE_INVALID] = "invalid",

};

static void board_list_phases(void)
{
	enum board_phase_t phase;

	for (phase = BOARD_PHASE_FIRST; phase < BOARD_PHASE_TEST; phase++)
		printf("%3d %s\n", gd->phase_count[phase], phase_name[phase]);
}

static int do_board(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int cmd = 'p';

	if (argc > 1)
		cmd = argv[1][0];

	switch (cmd) {
	case 'p': /* phases */
		board_list_phases();
		break;
	default:
		return CMD_RET_FAILURE;
	}

	return 0;
}

U_BOOT_CMD(
	board,	2,	0,	do_board,
	"Access board information",
	"phases\t- Show information about completed board init phases"
);
