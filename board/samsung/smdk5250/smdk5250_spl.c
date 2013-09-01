/*
 * Copyright (c) 2012 The Chromium OS Authors.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/arch/board.h>
#include <asm/arch/cpu.h>
#include <asm/arch/gpio.h>
#include <asm/arch/spl.h>
#include <asm/arch/clk.h>

/* Parameters of early board initialization in SPL */
static struct spl_machine_param machine_param
		__attribute__((section(".machine_param"))) = {
	.signature	= SPL_SIGNATURE,
	.version	= 1,
	.params		= "vmSouCbfasirRMwWjAUdDpctz",
	.size		= sizeof(machine_param),

	.mem_iv_size	= 0x1f,
	.mem_type	= DDR_MODE_DDR3,

	.uboot_start	= CONFIG_SYS_TEXT_BASE,
	.uboot_offset	= 48 << 10,
	/*
	 * Set uboot_size to 0x100000 bytes.
	 *
	 * This is an overly conservative value chosen to accommodate all
	 * possible U-Boot image.  You are advised to set this value to a
	 * smaller realistic size via scripts that modifies the .machine_param
	 * section of output U-Boot image.
	 */
	.uboot_size	= 0x100000,
	.uboot_comp_size = 0x100000,

	.boot_source	= BOOT_MODE_OM,
	.frequency_mhz	= 800,
	.arm_freq_mhz	= 1700,
	.serial_base	= 0x12c30000,
	.i2c_base	= 0x12c60000,
	.mem_manuf	= MEM_MANUF_SAMSUNG,
	.bad_wake_gpio	= 0xffffffff,
	.write_protect_gpio = 0xffffffff,
	.rtc_type	= SPL_RTC_TYPE_UNKNOWN,
	.compress_type	= SPL_COMPRESST_NONE,
};

struct spl_machine_param *spl_get_machine_params(void)
{
	if (machine_param.signature != SPL_SIGNATURE) {
		/* Will hang if SIGNATURE dont match */
		while (1)
			;
	}

	return &machine_param;
}
