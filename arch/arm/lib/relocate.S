/*
 *  relocate - common relocation function for ARM U-Boot
 *
 *  Copyright (c) 2013  Albert ARIBAUD <albert.u.boot@aribaud.net>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <linux/linkage.h>

/*
 * void relocate_code(addr_moni)
 *
 * This function relocates the monitor code.
 *
 * NOTE:
 * To prevent the code below from containing references with an R_ARM_ABS32
 * relocation record type, we never refer to linker-defined symbols directly.
 * Instead, we declare literals which contain their relative location with
 * respect to relocate_code, and at run time, add relocate_code back to them.
 */

ENTRY(relocate_code)
	mov	r6, r0	/* save addr of destination */

	ldr	r0, =__image_copy_start	/* r0 <- SRC &__image_copy_start */
	subs	r9, r6, r0		/* r9 <- relocation offset */
	beq	relocate_done		/* skip relocation */
	mov	r1, r6			/* r1 <- scratch for copy loop */
	adr	r7, relocate_code	/* r7 <- SRC &relocate_code */
	ldr	r2, =__image_copy_end	/* r2 <- SRC &__image_copy_end */

copy_loop:
	ldmia	r0!, {r10-r11}		/* copy from source address [r0]    */
	stmia	r1!, {r10-r11}		/* copy to   target address [r1]    */
	cmp	r0, r2			/* until source end address [r2]    */
	blo	copy_loop

	/*
	 * fix .rel.dyn relocations
	 */
	ldr	r2, =__rel_dyn_start	/* r2 <- SRC &__rel_dyn_start */
	ldr	r3, =__rel_dyn_end	/* r3 <- SRC &__rel_dyn_end */
fixloop:
	ldr	r0, [r2]		/* r0 <- SRC location to fix up */
	add	r0, r0, r9		/* r0 <- DST location to fix up */
	ldr	r1, [r2, #4]
	and	r7, r1, #0xff
	cmp	r7, #23			/* relative fixup? */
	beq	fixrel
	/* ignore unknown type of fixup */
	b	fixnext
fixrel:
	/* relative fix: increase location by offset */
	ldr	r1, [r0]
	add	r1, r1, r9
fixnext:
	str	r1, [r0]
	add	r2, r2, #8		/* each rel.dyn entry is 8 bytes */
	cmp	r2, r3
	blo	fixloop

relocate_done:

	/* ARMv4- don't know bx lr but the assembler fails to see that */

#ifdef __ARM_ARCH_4__
        mov        pc, lr
#else
        bx        lr
#endif

ENDPROC(relocate_code)
