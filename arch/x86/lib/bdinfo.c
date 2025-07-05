// SPDX-License-Identifier: GPL-2.0+
/*
 * x86-specific information for the 'bd' command
 *
 * Copyright 2021 Google LLC
 */

#include <cpu.h>
#include <display_options.h>
#include <efi.h>
#include <init.h>
#include <asm/cpu.h>
#include <asm/efi.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

void arch_print_bdinfo(void)
{
	lprint_num_l("prev table", gd->arch.table);
	lprint_num_l("clock_rate", gd->arch.clock_rate);
	lprint_num_l("tsc_base", gd->arch.tsc_base);
	lprint_num_l("vendor", gd->arch.x86_vendor);
	if (!IS_ENABLED(CONFIG_X86_64)) {
		char vendor_name[16];

		x86_cpu_vendor_info(vendor_name);
		lprint_str(" name", vendor_name);
	}
	lprint_num_l("model", gd->arch.x86_model);
	lprint_num_l("phys_addr in bits", cpu_phys_address_size());
	lprint_num_l("table start", gd->arch.table_start);
	lprint_num_l("table end", gd->arch.table_end);
	lprint_num_l(" high start", gd->arch.table_start_high);
	lprint_num_l(" high end", gd->arch.table_end_high);

	lprint_num_ll("tsc", rdtsc());

	if (IS_ENABLED(CONFIG_EFI_STUB))
		efi_show_bdinfo();
}
