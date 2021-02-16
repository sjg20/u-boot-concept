// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <command.h>
#include <asm/arch-coreboot/sysinfo.h>

static void show_table(struct sysinfo_t *info)
{
	printf("Coreboot table at %lx, decoded to %p\n",
	       gd->arch.coreboot_table, info);
}

static int do_cbsysinfo(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	if (!gd->arch.coreboot_table) {
		printf("No coreboot sysinfo table found\n");
		return CMD_RET_FAILURE;
	}
	show_table(&lib_sysinfo);

	return 0;
}

U_BOOT_CMD(
	cbsysinfo,	1,	1,	do_cbsysinfo,
	"Show coreboot sysinfo table",
	"Dumps out the contents of the sysinfo table. Thes only works if\n"
	"U-Boot is booted from coreboot"
);
