// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018, Bin Meng <bmeng.cn@gmail.com>
 *
 * U-Boot syscon driver for RISC-V Core Local Interruptor (CLINT)
 * The CLINT block holds memory-mapped control and status registers
 * associated with software and timer interrupts.
 */

#include <common.h>
#include <dm.h>
#include <regmap.h>
#include <syscon.h>
#include <asm/clint.h>
#include <asm/io.h>

/* MSIP registers */
#define MSIP_REG(base, hart)		((ulong)(base) + (hart) * 4)
/* Timer compare register */
#define MTIMECMP_REG(base, hart)	((ulong)(base) + 0x4000 + (hart) * 8)
/* Timer register */
#define MTIME_REG(base)			((ulong)(base) + 0xbff8)

static void __iomem *clint_base;

/*
 * The following 3 APIs are used to implement Supervisor Binary Interface (SBI)
 * as defined by the RISC-V privileged architecture spec v1.10.
 *
 * For performance reasons we don't want to get the CLINT register base via
 * syscon_get_first_range() each time we enter in those functions, instead
 * the base address was saved to a global variable during the clint driver
 * probe phase, so that we can use it directly.
 */

void riscv_send_ipi(int hart)
{
	writel(1, (void __iomem *)MSIP_REG(clint_base, hart));
}

void riscv_set_timecmp(int hart, u64 cmp)
{
	writeq(cmp, (void __iomem *)MTIMECMP_REG(clint_base, hart));
}

u64 riscv_get_time(void)
{
	return readq((void __iomem *)MTIME_REG(clint_base));
}

static int clint_probe(struct udevice *dev)
{
	clint_base = syscon_get_first_range(RISCV_SYSCON_CLINT);

	return 0;
}

static const struct udevice_id clint_ids[] = {
	{ .compatible = "riscv,clint0", .data = RISCV_SYSCON_CLINT },
	{ }
};

U_BOOT_DRIVER(riscv_clint) = {
	.name		= "riscv-clint",
	.id		= UCLASS_SYSCON,
	.of_match	= clint_ids,
	.probe		= clint_probe,
	.flags		= DM_FLAG_PRE_RELOC,
};
