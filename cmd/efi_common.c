// SPDX-License-Identifier: GPL-2.0+
/*
 * Common code for EFI commands
 *
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <efi.h>
#include <efi_api.h>
#include <u-boot/uuid.h>

void efi_show_tables(struct efi_system_table *systab)
{
	int i;

	printf("%.*s    Size  %-36s  Name", 2 * (int)sizeof(long), "Address",
	       "GUID");
	for (i = 0; i < systab->nr_tables; i++) {
		struct efi_configuration_table *tab = &systab->tables[i];
		u32 size = 0;

		/*
		 * The size is not in the configuration table entry itself.
		 * We must read it from the header of the actual table data.
		 * For most common tables (ACPI, SMBIOS, FDT), the size is
		 * a 32-bit integer at offset +4.
		 */
		if (tab->table) {
			// Cast to char* for byte-based pointer arithmetic
			size = *(u32 *)((char *)tab->table + 4);
		}

		printf("%p  %6x %pUl  %s\n", tab->table, size, tab->guid.b,
		       uuid_guid_get_str(tab->guid.b) ?: "(unknown)");
	}
}
