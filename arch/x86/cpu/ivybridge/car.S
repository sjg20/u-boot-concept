/*
 * Copyright (c) 2014 Google, Inc
 *
 * This file was copied from Coreboot file
 * cpu/intel/model_206ax/cache_as_ram.inc
 *
 * Copyright (C) 2000,2007 Ronald G. Minnich <rminnich@gmail.com>
 * Copyright (C) 2005 Tyan (written by Yinghai Lu for Tyan)
 * Copyright (C) 2007-2008 coresystems GmbH
 * Copyright (C) 2012 Kyösti Mälkki <kyosti.malkki@gmail.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <asm/mtrr.h>
#include <asm/post.h>
#include <asm/processor-flags.h>

#define MTRRphysBase_MSR(reg) (0x200 + 2 * (reg))
#define MTRRphysMask_MSR(reg) (0x200 + 2 * (reg) + 1)

#define CACHE_AS_RAM_SIZE	CONFIG_DCACHE_RAM_SIZE
#define CACHE_AS_RAM_BASE	CONFIG_DCACHE_RAM_BASE

/* Cache 4GB - MRC_SIZE_KB for MRC */
#define CACHE_MRC_BYTES	((CONFIG_CACHE_MRC_SIZE_KB << 10) - 1)
#define CACHE_MRC_BASE		(0xFFFFFFFF - CACHE_MRC_BYTES)
#define CACHE_MRC_MASK		(~CACHE_MRC_BYTES)

#define CPU_PHYSMASK_HI	(1 << (CONFIG_CPU_ADDR_BITS - 32) - 1)

#define NoEvictMod_MSR 0x2e0

	/* Note: ebp must not be touched in this code */
.globl car_init
car_init:
	post_code(0x20)

	/* Send INIT IPI to all excluding ourself. */
	movl	$0x000C4500, %eax
	movl	$0xFEE00300, %esi
	movl	%eax, (%esi)

	/* All CPUs need to be in Wait for SIPI state */
wait_for_sipi:
	movl	(%esi), %eax
	bt	$12, %eax
	jc	wait_for_sipi

	post_code(0x21)
	/* Zero out all fixed range and variable range MTRRs. */
	movl	$mtrr_table, %esi
	movl	$((mtrr_table_end - mtrr_table) / 2), %edi
	xorl	%eax, %eax
	xorl	%edx, %edx
clear_mtrrs:
	movw	(%esi), %bx
	movzx	%bx, %ecx
	wrmsr
	add	$2, %esi
	dec	%edi
	jnz	clear_mtrrs

	post_code(0x22)
	/* Configure the default memory type to uncacheable. */
	movl	$MTRRdefType_MSR, %ecx
	rdmsr
	andl	$(~0x00000cff), %eax
	wrmsr

	post_code(0x23)
	/* Set Cache-as-RAM base address. */
	movl	$(MTRRphysBase_MSR(0)), %ecx
	movl	$(CACHE_AS_RAM_BASE | MTRR_TYPE_WRBACK), %eax
	xorl	%edx, %edx
	wrmsr

	post_code(0x24)
	/* Set Cache-as-RAM mask. */
	movl	$(MTRRphysMask_MSR(0)), %ecx
	movl	$(~(CACHE_AS_RAM_SIZE - 1) | MTRRphysMaskValid), %eax
	movl	$CPU_PHYSMASK_HI, %edx
	wrmsr

	post_code(0x25)

	/* Enable MTRR. */
	movl	$MTRRdefType_MSR, %ecx
	rdmsr
	orl	$MTRRdefTypeEn, %eax
	wrmsr

	/* Enable cache (CR0.CD = 0, CR0.NW = 0). */
        movl	%cr0, %eax
	andl	$(~(X86_CR0_CD | X86_CR0_NW)), %eax
	invd
	movl	%eax, %cr0

	/* enable the 'no eviction' mode */
	movl    $NoEvictMod_MSR, %ecx
	rdmsr
	orl     $1, %eax
	andl    $~2, %eax
	wrmsr

       /* Clear the cache memory region. This will also fill up the cache */
	movl	$CACHE_AS_RAM_BASE, %esi
	movl	%esi, %edi
	movl	$(CACHE_AS_RAM_SIZE / 4), %ecx
	// movl	$0x23322332, %eax
	xorl	%eax, %eax
	rep	stosl

	/* enable the 'no eviction run' state */
	movl    $NoEvictMod_MSR, %ecx
	rdmsr
	orl     $3, %eax
	wrmsr

	post_code(0x26)
	/* Enable Cache-as-RAM mode by disabling cache. */
	movl	%cr0, %eax
	orl	$X86_CR0_CD, %eax
	movl	%eax, %cr0

	/* Enable cache for our code in Flash because we do XIP here */
	movl	$MTRRphysBase_MSR(1), %ecx
	xorl	%edx, %edx
	/*
	 * IMPORTANT: The following calculation _must_ be done at runtime. See
	 * http://www.coreboot.org/pipermail/coreboot/2010-October/060855.html
	 */
	movl    $car_init_ret, %eax
	andl    $(~(CONFIG_XIP_ROM_SIZE - 1)), %eax
	orl	$MTRR_TYPE_WRPROT, %eax
	wrmsr

	movl	$MTRRphysMask_MSR(1), %ecx
	movl	$CPU_PHYSMASK_HI, %edx
	movl	$(~(CONFIG_XIP_ROM_SIZE - 1) | MTRRphysMaskValid), %eax
	wrmsr

	post_code(0x27)
#ifdef CONFIG_CACHE_MRC_BIN
	/* Enable caching for ram init code to run faster */
	movl	$MTRRphysBase_MSR(2), %ecx
	movl	$(CACHE_MRC_BASE | MTRR_TYPE_WRPROT), %eax
	xorl	%edx, %edx
	wrmsr
	movl	$MTRRphysMask_MSR(2), %ecx
	movl	$(CACHE_MRC_MASK | MTRRphysMaskValid), %eax
	movl	$CPU_PHYSMASK_HI, %edx
	wrmsr
#endif

	post_code(0x28)
	/* Enable cache. */
	movl	%cr0, %eax
	andl	$(~(X86_CR0_CD | X86_CR0_NW)), %eax
	movl	%eax, %cr0

	post_code(0x29)

	/* return */
	jmp	car_init_ret


#if 0 /* Not currently used */
disable_car:
	post_code(0x2f)

	/* Copy global variable space (for USBDEBUG) to memory */
#if CONFIG_USBDEBUG
	cld
	movl	$(CONFIG_DCACHE_RAM_BASE + CONFIG_DCACHE_RAM_SIZE - 24), %esi
	movl	$(CONFIG_RAMTOP - 24), %edi
	movl	$24, %ecx
	rep	movsb
#endif

	post_code(0x30)

	/* Disable cache. */
	movl	%cr0, %eax
	orl	$X86_CR0_CD, %eax
	movl	%eax, %cr0

	post_code(0x31)

	/* Disable MTRR. */
	movl	$MTRRdefType_MSR, %ecx
	rdmsr
	andl	$(~MTRRdefTypeEn), %eax
	wrmsr

	post_code(0x31)

	/* Disable the no eviction run state */
	movl    $NoEvictMod_MSR, %ecx
	rdmsr
	andl    $~2, %eax
	wrmsr

	invd

	/* Disable the no eviction mode */
	rdmsr
	andl    $~1, %eax
	wrmsr

#ifdef CONFIG_CACHE_MRC_BIN
	/* Clear MTRR that was used to cache MRC */
	xorl	%eax, %eax
	xorl	%edx, %edx
	movl	$MTRRphysBase_MSR(2), %ecx
	wrmsr
	movl	$MTRRphysMask_MSR(2), %ecx
	wrmsr
#endif

	post_code(0x33)

	/* Enable cache. */
	movl	%cr0, %eax
	andl	$~(X86_CR0_CD | X86_CR0_NW), %eax
	movl	%eax, %cr0

	post_code(0x36)

	/* Disable cache. */
	movl	%cr0, %eax
	orl	X86_CR0_CD, %eax
	movl	%eax, %cr0

	post_code(0x38)

	/* Enable Write Back and Speculative Reads for the first MB
	 * and ramstage.
	 */
	movl	$MTRRphysBase_MSR(0), %ecx
	movl	$(0x00000000 | MTRR_TYPE_WRBACK), %eax
	xorl	%edx, %edx
	wrmsr
	movl	$MTRRphysMask_MSR(0), %ecx
	movl	$(~(CONFIG_RAMTOP - 1) | MTRRphysMaskValid), %eax
	movl	$CPU_PHYSMASK_HI, %edx	// 36bit address space
	wrmsr

#if CONFIG_CACHE_ROM_SIZE
	/* Enable Caching and speculative Reads for the
	 * complete ROM now that we actually have RAM.
	 */
	movl	$MTRRphysBase_MSR(1), %ecx
	movl	$(CACHE_ROM_BASE | MTRR_TYPE_WRPROT), %eax
	xorl	%edx, %edx
	wrmsr
	movl	$MTRRphysMask_MSR(1), %ecx
	movl	$(~(CONFIG_CACHE_ROM_SIZE - 1) | MTRRphysMaskValid), %eax
	movl	$CPU_PHYSMASK_HI, %edx
	wrmsr
#endif

	post_code(0x39)

	/* And enable cache again after setting MTRRs. */
	movl	%cr0, %eax
	andl	$~(X86_CR0_CD | X86_CR0_NW), %eax
	movl	%eax, %cr0

	post_code(0x3a)

	/* Enable MTRR. */
	movl	$MTRRdefType_MSR, %ecx
	rdmsr
	orl	$MTRRdefTypeEn, %eax
	wrmsr

	post_code(0x3b)

	/* Invalidate the cache again. */
	invd

	post_code(0x3c)

#if CONFIG_HAVE_ACPI_RESUME
	movl	CBMEM_BOOT_MODE, %eax
	cmpl	$0x2, %eax // Resume?
	jne	__acpi_resume_backup_done

	/* copy 1MB - 64K to high tables ram_base to prevent memory corruption
	 * through stage 2. We could keep stuff like stack and heap in high
	 * tables memory completely, but that's a wonderful clean up task for
	 * another day.
	 */
	cld
	movl	$CONFIG_RAMBASE, %esi
	movl	CBMEM_RESUME_BACKUP, %edi
	movl	$HIGH_MEMORY_SAVE / 4, %ecx
	rep	movsl

__acpi_resume_backup_done:
#endif

	post_code(0x3d)

	/* TODO: Return properly */
	ret
#endif

.Lhlt:
	post_code(POST_DEAD_CODE)
	hlt
	jmp	.Lhlt

mtrr_table:
	/* Fixed MTRRs */
	.word 0x250, 0x258, 0x259
	.word 0x268, 0x269, 0x26A
	.word 0x26B, 0x26C, 0x26D
	.word 0x26E, 0x26F
	/* Variable MTRRs */
	.word 0x200, 0x201, 0x202, 0x203
	.word 0x204, 0x205, 0x206, 0x207
	.word 0x208, 0x209, 0x20A, 0x20B
	.word 0x20C, 0x20D, 0x20E, 0x20F
	.word 0x210, 0x211, 0x212, 0x213
mtrr_table_end:

