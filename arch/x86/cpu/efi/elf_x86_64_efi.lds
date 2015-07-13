/*
 * U-Boot EFI link script
 *
 * SPDX-License-Identifier:	bsd-2-clause
 *
 * Modified from usr/lib32/elf_ia32_efi.lds in gnu-efi
 */

#include <config.h>

OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)
SECTIONS
{
	. = 0x0;
	ImageBase = .;
	.hash : { *(.hash) }	/* this MUST come first! */
	. = ALIGN(4096);
	.eh_frame : {
		*(.eh_frame)
	}

	. = ALIGN(4096);

	.text : {
		*(.text)
		*(.text.*)
		*(.gnu.linkonce.t.*)
	}

	. = ALIGN(4096);

	.reloc : {
		*(.reloc)
	}

	. = ALIGN(4096);

	.data : {
		*(.rodata*)
		*(.got.plt)
		*(.got)
		*(.data*)
		*(.sdata)
		/* the EFI loader doesn't seem to like a .bss section, so we stick
		 * it all into .data: */
		*(.sbss)
		*(.scommon)
		*(.dynbss)
		*(.bss)
		*(COMMON)
		*(.rel.local)

		/* U-Boot lists and device tree */
		. = ALIGN(8);
		KEEP(*(SORT(.u_boot_list*)));
		. = ALIGN(8);
		KEEP(*(.dtb*));
		/* Keep U-Boot payload */
		. = ALIGN(8);
		KEEP(*(.u_boot_bin.*));
	}

	. = ALIGN(4096);
	.dynamic : { *(.dynamic) }
	. = ALIGN(4096);

	.rela : {
		*(.rela.data*)
		*(.rela.got)
		*(.rela.stab)
	}

	. = ALIGN(4096);
	.dynsym : { *(.dynsym) }
	. = ALIGN(4096);
	.dynstr : { *(.dynstr) }
	. = ALIGN(4096);
	.ignored.reloc : {
		*(.rela.reloc)
		*(.eh_frame)
		*(.note.GNU-stack)
	}

	.comment 0 : { *(.comment) }
}
