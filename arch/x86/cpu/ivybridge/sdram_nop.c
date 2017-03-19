/*
 * Copyright (c) 2016 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <asm/arch/sandybridge.h>

DECLARE_GLOBAL_DATA_PTR;

int nop_dram_init(void)
{
	gd->ram_size = 1ULL << 31;
	gd->bd->bi_dram[0].start = 0;
	gd->bd->bi_dram[0].size = gd->ram_size;

	return 0;
}

#ifndef CONFIG_BOARD_ENABLE
int dram_init(void)
{
	return nop_dram_init();
}
#endif

static int cpu_x86_nop_phase(struct udevice *dev, enum board_phase_t phase)
{
	switch (phase) {
	case BOARD_F_DRAM_INIT:
		return nop_dram_init();
	default:
		return -ENOSYS;
	}

	return 0;
}

static int cpu_x86_nop_board_probe(struct udevice *dev)
{
	return board_support_phase(dev, BOARD_F_DRAM_INIT);
}

static const struct board_ops cpu_x86_nop_board_ops = {
	.phase		= cpu_x86_nop_phase,
};

U_BOOT_DRIVER(cpu_x86_nop_board_drv) = {
	.name		= "cpu_x86_nop_board",
	.id		= UCLASS_BOARD,
	.ops		= &cpu_x86_nop_board_ops,
	.probe		= cpu_x86_nop_board_probe,
	.flags		= DM_FLAG_PRE_RELOC,
};

U_BOOT_DEVICE(cpu_x86_nop_board) = {
	.name		= "cpu_x86_nop_board",
};
