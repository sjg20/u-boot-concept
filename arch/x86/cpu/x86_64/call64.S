/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2020 Intel Corporation <www.intel.com>
 */

.code64
.globl cpu_call64
cpu_call64:
	/*
	 * cpu_call64(ulong pgtable, ulong setup_base, ulong target)
	 *
	 * rdi - pgtable (unused - already in 64-bit with paging)
	 * rsi - setup_base
	 * rdx - target
	 *
	 */
	cli
	mov	%rdx, %rcx
	mov	%rsi, %rdx
	call	*%rcx
	ret
