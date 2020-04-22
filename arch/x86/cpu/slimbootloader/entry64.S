/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2020 Intel Corporation <www.intel.com>
 */

#include <generated/asm-offsets.h>

.section .text

.globl init_64bit_entry
init_64bit_entry:
	/* Save hob pointer parameter */
	mov	%rcx, %r10
	jmp	init_64bit_entry_ret
