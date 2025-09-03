/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Linker script for ulib_test_static binary
 * Ensures proper alignment for U-Boot linker lists when linking with libu-boot.a
 */

SECTIONS
{
	/* Ensure proper alignment for linker lists */
	. = ALIGN(32);
	__u_boot_list : {
		__u_boot_list_start = .;
		KEEP(*(SORT(__u_boot_list*)));
		__u_boot_list_end = .;
	}

	/* Sandbox getopt section */
	. = ALIGN(8);
	_u_boot_sandbox_getopt : {
		*(_u_boot_sandbox_getopt_start)
		KEEP(*(_u_boot_sandbox_getopt))
		*(_u_boot_sandbox_getopt_end)
	}

	/* EFI runtime sections (if needed) */
	. = ALIGN(8);
	efi_runtime : {
		__efi_runtime_start = .;
		*(efi_runtime_text)
		*(efi_runtime_data)
		__efi_runtime_stop = .;
	}

	. = ALIGN(8);
	efi_runtime_rel : {
		__efi_runtime_rel_start = .;
		*(.relefi_runtime_text)
		*(.relefi_runtime_data)
		__efi_runtime_rel_stop = .;
	}
}

INSERT AFTER .rodata;