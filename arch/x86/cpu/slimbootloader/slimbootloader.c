// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2019-2020 Intel Corporation <www.intel.com>
 */

#include <common.h>
#include <cpu_func.h>
#include <asm/arch/slimbootloader.h>

DECLARE_GLOBAL_DATA_PTR;

/**
 * This sets tsc_base and clock_rate for early_timer and tsc_timer.
 * The performance info guid hob has all performance timestamp data, but
 * the only tsc frequency info is used for the timer driver for now.
 *
 * Slim Bootloader already calibrated TSC and provides it to U-Boot.
 * Therefore, U-Boot does not have to re-calibrate TSC.
 * Configuring tsc_base and clock_rate here makes x86 tsc_timer driver
 * bypass TSC calibration and use the provided TSC frequency.
 */
static void tsc_init(void)
{
	struct sbl_performance_info *data;
	const efi_guid_t guid = SBL_PERFORMANCE_INFO_GUID;

	if (!gd->arch.hob_list)
		panic("hob list not found!");

	gd->arch.tsc_base = rdtsc();
	debug("tsc_base=0x%llx\n", gd->arch.tsc_base);

	data = hob_get_guid_hob_data(gd->arch.hob_list, NULL, &guid);
	if (!data) {
		debug("performance info hob not found\n");
		return;
	}

	/* frequency is in KHz, so to Hz */
	gd->arch.clock_rate = data->frequency * 1000;
	debug("freq=0x%lx\n", gd->arch.clock_rate);
}

int arch_cpu_init(void)
{
	int ret = 0;

	tsc_init();

#if !CONFIG_IS_ENABLED(X86_64)
	ret = x86_cpu_init_f();
#endif
	return ret;
}

#if CONFIG_IS_ENABLED(X86_64)
int set_hob_list(void *hob_list)
{
	gd->arch.hob_list = hob_list;
	return 0;
}
#else
int checkcpu(void)
{
	return 0;
}

int print_cpuinfo(void)
{
	return default_print_cpuinfo();
}
#endif
