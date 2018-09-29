/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2018, Bin Meng <bmeng.cn@gmail.com>
 */

#ifndef _ASM_RISCV_CLINT_H
#define _ASM_RISCV_CLINT_H

/*
 * System controllers in a RISC-V system
 *
 * So far only Core Local Interruptor (CLINT) is defined. If new system
 * controller is added, we may need move the defines to somewhere else.
 */
enum {
	RISCV_NONE,
	RISCV_SYSCON_CLINT,	/* Core Local Interruptor (CLINT) */
};

void riscv_send_ipi(int hart);
void riscv_set_timecmp(int hart, u64 cmp);
u64 riscv_get_time(void);

#endif /* _ASM_RISCV_CLINT_H */
