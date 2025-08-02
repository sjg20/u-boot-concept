/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * U-Boot aarch64 EFI linker script
 *
 * Modified from elf_aarch64_efi.lds in gnu-efi
 */

OUTPUT_FORMAT("elf64-littleaarch64", "elf64-littleaarch64", "elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)
SECTIONS
{
	. = 0;
	image_base = .;
	/* .hash and/or .gnu.hash MUST come first! */
	.hash : { *(.hash) }
	.gnu.hash : { *(.gnu.hash) }
	. = ALIGN(4096);
	.eh_frame : { *(.eh_frame) }
	. = ALIGN(4096);
	.text : {
		_text = .;
		*(.text)
		*(.text.*)
		*(.gnu.linkonce.t.*)
		*(.srodata)
		*(.rodata*)
		. = ALIGN(16);
		*(.dynamic);
		. = ALIGN(512);
	}
	_etext = .;
	_text_size = . - _text;
	. = ALIGN(65536);
	.reloc : {
		KEEP (*(.reloc))
	}
	. = ALIGN(4096);
	.data : {
		_data = .;
		*(.sdata)
		*(.data)
		*(.data1)
		*(.data.*)
		*(.got.plt)
		*(.got)
		/* U-Boot lists and device tree */
		. = ALIGN(8);
		*(SORT(__u_boot_list*));
		. = ALIGN(8);

		/*
		 * The EFI loader doesn't seem to like a .bss section, so we
		 * stick it all into .data:
		 */
		. = ALIGN(16);
		_bss = .;
		*(.sbss)
		*(.scommon)
		*(.dynbss)
		*(.bss)
		*(.bss.*)
		*(COMMON)
		. = ALIGN(512);
		_bss_end = .;
		_edata = .;
		_end = .;

		/* This needs to come before *(.text*) */
                __efi_runtime_start = .;
		*(.text.efi_runtime*)
		*(.rodata.efi_runtime*)
		*(.data.efi_runtime*)
                __efi_runtime_stop = .;

	}

	_data_size = _edata - _data;

	. = ALIGN(4096);
	.rela : {
		*(.rela.text*)
		*(.rela.data*)
		*(.rela.got)
		*(.rela.dyn)
		*(.rela.stab)
		*(.rela.init_array*)
		*(.rela.fini_array*)
		*(.rela.ctors*)
		*(.rela.dtors*)
		*(.rela__u_boot_list*)
	}

	. = ALIGN(8);
	.efi_runtime_rel : {
                __efi_runtime_rel_start = .;
		*(.rel*.efi_runtime)
		*(.rel*.efi_runtime.*)
                __efi_runtime_rel_stop = .;
	}

	. = ALIGN(4096);
	.dynsym   : { *(.dynsym) }
	. = ALIGN(4096);
	.dynstr   : { *(.dynstr) }
	. = ALIGN(4096);
	.note.gnu.build-id : { *(.note.gnu.build-id) }
	/DISCARD/ : {
		*(.rel.reloc)
		*(.eh_frame)
		*(.note.GNU-stack)
	}
	.embedded_dtb : {
		*(.embedded_dtb)
	}
	.comment 0 : { *(.comment) }
}
