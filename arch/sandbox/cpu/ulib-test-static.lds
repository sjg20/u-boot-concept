/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Linker script for ulib_test_static binary
 *
 * This ensures proper alignment for linker-lists when linking with libu-boot.a
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
}

INSERT AFTER .rodata;
