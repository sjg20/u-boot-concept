/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 *
 * Linker script for U-Boot shared library (libu-boot.so)
 */

SECTIONS
{
	/* Text sections first */
	.text : {
		*(.text*)
	}

	. = ALIGN(32);
	__u_boot_list : {
		KEEP(*(SORT(__u_boot_list*)));
	}

	_u_boot_sandbox_getopt : {
		*(_u_boot_sandbox_getopt_start)
		KEEP(*(_u_boot_sandbox_getopt))
		*(_u_boot_sandbox_getopt_end)
	}

	/* Read-only data */
	. = ALIGN(8);
	.rodata : { *(SORT_BY_ALIGNMENT(SORT_BY_NAME(.rodata*))) }

	/* Data sections */
	. = ALIGN(8);
	.data : {
		*(.data*)
	}

	/* BSS section */
	. = ALIGN(8);
	.bss : {
		__bss_start = .;
		*(.bss*)
		*(COMMON)
		. = ALIGN(8);
		__bss_end = .;
	}

	/* Dynamic linking sections for shared library */
	.dynamic : { *(.dynamic) }
	.dynsym : {
		__dyn_sym_start = .;
		*(.dynsym)
		__dyn_sym_end = .;
	}
	.dynstr : { *(.dynstr) }
	.gnu.hash : { *(.gnu.hash) }
	.hash : { *(.hash) }
	.got : { *(.got) }
	.got.plt : { *(.got.plt) }
	.plt : { *(.plt) }
	.rela.dyn : { *(.rela.dyn) }
	.rela.plt : { *(.rela.plt) }

	/* Debugging sections */
	.debug_info 0 : { *(.debug_info) }
	.debug_abbrev 0 : { *(.debug_abbrev) }
	.debug_aranges 0 : { *(.debug_aranges) }
	.debug_ranges 0 : { *(.debug_ranges) }
	.debug_line 0 : { *(.debug_line) }
	.debug_str 0 : { *(.debug_str) }
	.debug_frame 0 : { *(.debug_frame) }
	.debug_loc 0 : { *(.debug_loc) }

	/DISCARD/ : { *(.note*) }
}
