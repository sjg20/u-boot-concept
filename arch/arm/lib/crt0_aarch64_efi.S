/* SPDX-License-Identifier: GPL-2.0+ OR BSD-2-Clause */
/*
 * crt0-efi-aarch64.S - PE/COFF header for aarch64 EFI applications
 *
 * Copright (C) 2014 Linaro Ltd. <ard.biesheuvel@linaro.org>
 *
 *
 * This file is taken and modified from the gnu-efi project.
 */

#include <asm-generic/pe.h>

	.text
	.align		12

	.globl _start
	.type _start,%function
_start:
	stp		x29, x30, [sp, #-32]!
	mov		x29, sp

	stp		x0, x1, [sp, #16]
	# adr		x0, image_base
	adrp		x1, _DYNAMIC
	add		x1, x1, #:lo12:_DYNAMIC
	bl		_relocate
	cbnz		x0, 0f

	ldp		x0, x1, [sp, #16]
	# bl		efi_main

0:	ldp		x29, x30, [sp], #32
	ret

	/*
	 * hand-craft a dummy .reloc section so EFI knows it's a relocatable
	 * executable:
	*/
	.data
dummy:	.4byte	0

#define IMAGE_REL_ABSOLUTE	0
	.section .reloc, "a"
label1:
	.4byte	dummy-label1			// Page RVA
	.4byte	12				// Block Size (2*4+2*2), must be aligned by 32 Bits
	.2byte	(IMAGE_REL_ABSOLUTE<<12) +  0	// reloc for dummy
	.2byte	(IMAGE_REL_ABSOLUTE<<12) +  0	// reloc for dummy

#if defined(__ELF__) && defined(__linux__)
	.section .note.GNU-stack,"",%progbits
#endif
