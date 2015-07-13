/*
 * (C) Copyright 2015 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/global_data.h>
#include <asm/msr-index.h>
#include <asm/processor-flags.h>

.code64
.globl cpu_call32
cpu_call32:
	/*
	 * cpu_call32(ulong target)
	 *
	 * eax - target?
	 */
	cli
	push 0x10	/* selector of the compatibility code segment */
	xor rcx,rcx

	mov ecx,Back32    ; The address must be an 64-bit address,
			; so upper 32-bits of RCX are zero.
	push rcx
	retf

	.data
gdt:
	.word	gdt_end - gdt
	.long	gdt
	.word	0
	.quad	0x0000000000000000	/* NULL descriptor */
	.quad	0x00af9a000000ffff	/* __KERNEL_CS */
	.quad	0x00cf92000000ffff	/* __KERNEL_DS */
	.quad	0x0080890000000000	/* TS descriptor */
	.quad   0x0000000000000000	/* TS continued */
gdt_end:
