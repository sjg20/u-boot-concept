// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018, Bin Meng <bmeng.cn@gmail.com>
 *
 * RISC-V privileged architecture timer
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <syscon.h>
#include <timer.h>
#include <asm/io.h>
#include <asm/clint.h>

static int riscv_timer_get_count(struct udevice *dev, u64 *count)
{
	*count = riscv_get_time();

	return 0;
}

static int riscv_timer_probe(struct udevice *dev)
{
	struct timer_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct udevice *clint;
	int ret;

	/* make sure clint driver is loaded */
	ret = syscon_get_by_driver_data(RISCV_SYSCON_CLINT, &clint);
	if (ret)
		return ret;

	/* clock frequency was passed from the cpu driver as driver data */
	uc_priv->clock_rate = dev->driver_data;

	return 0;
}

static const struct timer_ops riscv_timer_ops = {
	.get_count = riscv_timer_get_count,
};

U_BOOT_DRIVER(riscv_timer) = {
	.name = "riscv_timer",
	.id = UCLASS_TIMER,
	.probe = riscv_timer_probe,
	.ops = &riscv_timer_ops,
	.flags = DM_FLAG_PRE_RELOC,
};
