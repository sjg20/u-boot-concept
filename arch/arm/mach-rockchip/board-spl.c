/*
 * (C) Copyright 2015 Google, Inc
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <fdtdec.h>
#include <malloc.h>
#include <spl.h>
#include <asm/io.h>
#include <asm/arch/grf.h>
#include <dm/root.h>

DECLARE_GLOBAL_DATA_PTR;

#define RK_CLRSETBITS(clr, set) ((((clr) | (set)) << 16) | set)
#define IOMUX_UART2	RK_CLRSETBITS(7 << 12 | 3 << 8, 1 << 12 | 1 << 8)
#define GRF_BASE		0xFF770000

u32 spl_boot_device(void)
{
	return BOOT_DEVICE_SPI;
}

void spl_board_load_image(void)
{
}

void board_init_f(ulong dummy)
{
	struct rk3288_grf_regs * const rk3288_grf = (void *)GRF_BASE;

	writel(IOMUX_UART2, &rk3288_grf->iomux_uart2);

	/*
	 * Debug UART can be used from here if required:
	 *
	 * debug_uart_init();
	 * printch('a');
	 * printhex8(0x1234);
	 * printascii("string");
	 */

	/* Clear the BSS */
	memset(__bss_start, 0, __bss_end - __bss_start);

	board_init_r(NULL, 0);
}

void spl_board_init(void)
{
	preloader_console_init();
}
