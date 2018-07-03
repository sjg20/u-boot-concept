// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2017 Intel Corporation
 */

#include <common.h>
#include <asm/scu.h>
#include <asm/u-boot-x86.h>

/*
 * Miscellaneous platform dependent initializations
 */
int arch_cpu_init(void)
{
	return x86_cpu_init_f();
}

int checkcpu(void)
{
	return 0;
}

int print_cpuinfo(void)
{
	return default_print_cpuinfo();
}

/* TODO: convert to DM sysreset */
void reset_cpu(ulong addr)
{
	scu_ipc_simple_command(IPCMSG_COLD_RESET, 0);
}

int do_reset(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	reset_cpu(0);

	return 0;
}
