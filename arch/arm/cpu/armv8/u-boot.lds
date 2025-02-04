/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2013
 * David Feng <fenghua@phytium.com.cn>
 *
 * (C) Copyright 2002
 * Gary Jennejohn, DENX Software Engineering, <garyj@denx.de>
 */

#include <config.h>
#include <asm/psci.h>

OUTPUT_FORMAT("elf64-littleaarch64", "elf64-littleaarch64", "elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)
SECTIONS
{
#ifdef CONFIG_ARMV8_SECURE_BASE
	/DISCARD/ : { *(.rela._secure*) }
#endif
	. = 0x00000000;

	. = ALIGN(8);
	__image_copy_start = ADDR(.text);
	.text :
	{
		CPUDIR/start.o (.text*)
	}

	/* This needs to come before *(.text*) */
	.efi_runtime : {
                __efi_runtime_start = .;
		*(.text.efi_runtime*)
		*(.rodata.efi_runtime*)
		*(.data.efi_runtime*)
                __efi_runtime_stop = .;
	}

	.text_rest ALIGN(4096) :
	{
		__text_start = .;
		*(.text*)
		. = ALIGN(4096);
		__text_end = .;
	}

#ifdef CONFIG_ARMV8_PSCI
	.__secure_start :
#ifndef CONFIG_ARMV8_SECURE_BASE
		ALIGN(CONSTANT(COMMONPAGESIZE))
#endif
	{
		KEEP(*(.__secure_start))
	}

#ifndef CONFIG_ARMV8_SECURE_BASE
#define __ARMV8_SECURE_BASE
#define __ARMV8_PSCI_STACK_IN_RAM
#else
#define __ARMV8_SECURE_BASE	CONFIG_ARMV8_SECURE_BASE
#endif
	.secure_text __ARMV8_SECURE_BASE :
		AT(ADDR(.__secure_start) + SIZEOF(.__secure_start))
	{
		*(._secure.text)
		. = ALIGN(8);
		__secure_svc_tbl_start = .;
		KEEP(*(._secure_svc_tbl_entries))
		__secure_svc_tbl_end = .;
	}

	.secure_data : AT(LOADADDR(.secure_text) + SIZEOF(.secure_text))
	{
		*(._secure.data)
	}

	.secure_stack ALIGN(ADDR(.secure_data) + SIZEOF(.secure_data),
			    CONSTANT(COMMONPAGESIZE)) (NOLOAD) :
#ifdef __ARMV8_PSCI_STACK_IN_RAM
		AT(ADDR(.secure_stack))
#else
		AT(LOADADDR(.secure_data) + SIZEOF(.secure_data))
#endif
	{
		KEEP(*(.__secure_stack_start))

		. = . + CONFIG_ARMV8_PSCI_NR_CPUS * ARM_PSCI_STACK_SIZE;

		. = ALIGN(CONSTANT(COMMONPAGESIZE));

		KEEP(*(.__secure_stack_end))
	}

#ifndef __ARMV8_PSCI_STACK_IN_RAM
	. = LOADADDR(.secure_stack);
#endif

	.__secure_end : AT(ADDR(.__secure_end)) {
		KEEP(*(.__secure_end))
		LONG(0x1d1071c);	/* Must output something to reset LMA */
	}
#endif

	__start_rodata = ADDR(.rodata);
	.rodata ALIGN(4096): {
	    *(SORT_BY_ALIGNMENT(SORT_BY_NAME(.rodata*)))
	}

	__u_boot_list : {
		KEEP(*(SORT(__u_boot_list*)));
	}

	__end_rodata = ALIGN(4096);

	.efi_runtime_rel : {
                __efi_runtime_rel_start = .;
		*(.rel*.efi_runtime)
		*(.rel*.efi_runtime.*)
                __efi_runtime_rel_stop = .;
	}

	 __start_data = ADDR(.data);
	.data ALIGN(4096): {
		*(.data*)
	}

	. = ALIGN(8);
	__image_copy_end = .;

	/*
	 * arch/arm/lib/crt0_64.S assumes __bss_start - __bss_end % 8 == 0
	 */
	.bss ALIGN(8) : {
		__bss_start = .;
		*(.bss*)
		__bss_end = .;
	}
	__end_data = ALIGN(4096);

	.rela.dyn : {
		__rel_dyn_start = .;
		*(.rela*)
		__rel_dyn_end = .;
	}

	_end = .;

	/DISCARD/ : { *(.dynsym) }
	/DISCARD/ : { *(.dynstr*) }
	/DISCARD/ : { *(.dynamic*) }
	/DISCARD/ : { *(.plt*) }
	/DISCARD/ : { *(.interp*) }
	/DISCARD/ : { *(.gnu*) }

#ifdef CONFIG_LINUX_KERNEL_IMAGE_HEADER
#include "linux-kernel-image-header-vars.h"
#endif
}
