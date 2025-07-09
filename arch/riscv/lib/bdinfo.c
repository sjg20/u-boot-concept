// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * RISC-V-specific information for the 'bdinfo' command
 */

#include <display_options.h>
#include <init.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

void arch_print_bdinfo(void)
{
	lprint_num_l("boot hart", gd->arch.boot_hart);

	if (gd->arch.firmware_fdt_addr)
		lprint_num_ll("firmware fdt",
			      (long long)gd->arch.firmware_fdt_addr);
}
