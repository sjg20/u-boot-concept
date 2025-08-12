// SPDX-License-Identifier: GPL-2.0+
/*
 * Functions shared by the app and stub
 *
 * Copyright (c) 2015 Google, Inc
 *
 * EFI information obtained here:
 * http://wiki.phoenix.com/wiki/index.php/EFI_BOOT_SERVICES
 *
 * Common EFI functions
 */

#include <debug_uart.h>
#include <malloc.h>
#include <linux/err.h>
#include <linux/types.h>
#include <efi.h>
#include <efi_api.h>

enum {
	/* Maximum different attribute values we can track */
	ATTR_SEEN_MAX	= 30,
};

static const char *const type_name[] = {
	"reserved",
	"loader_code",
	"loader_data",
	"bs_code",
	"bs_data",
	"rt_code",
	"rt_data",
	"conv",
	"unusable",
	"acpi_reclaim",
	"acpi_nvs",
	"io",
	"io_port",
	"pal_code",
};

static struct attr_info {
	u64 val;
	const char *name;
} mem_attr[] = {
	{ EFI_MEMORY_UC, "uncached" },
	{ EFI_MEMORY_WC, "write-coalescing" },
	{ EFI_MEMORY_WT, "write-through" },
	{ EFI_MEMORY_WB, "write-back" },
	{ EFI_MEMORY_UCE, "uncached & exported" },
	{ EFI_MEMORY_WP, "write-protect" },
	{ EFI_MEMORY_RP, "read-protect" },
	{ EFI_MEMORY_XP, "execute-protect" },
	{ EFI_MEMORY_NV, "non-volatile" },
	{ EFI_MEMORY_MORE_RELIABLE, "higher reliability" },
	{ EFI_MEMORY_RO, "read-only" },
	{ EFI_MEMORY_SP, "specific purpose" },
	{ EFI_MEMORY_RUNTIME, "needs runtime mapping" }
};

const char *efi_mem_type_name(enum efi_memory_type type)
{
	return type < ARRAY_SIZE(type_name) ? type_name[type] : "<invalid>";
}

void efi_dump_mem_table(struct efi_mem_desc *desc, int size, int desc_size,
			bool skip_bs)
{
	u64 attr_seen[ATTR_SEEN_MAX];
	struct efi_mem_desc *end;
	int attr_seen_count;
	int upto, i;
	u64 addr;

	printf(" #  %-14s  %10s  %10s  %10s  %s\n", "Type", "Physical",
	       "Virtual", "Size", "Attributes");

	/* Keep track of all the different attributes we have seen */
	end = (struct efi_mem_desc *)((ulong)desc + size);
	attr_seen_count = 0;
	addr = 0;
	for (upto = 0; desc < end;
	     upto++, desc = efi_get_next_mem_desc(desc, desc_size)) {
		u64 size;

		if (skip_bs && efi_mem_is_boot_services(desc->type))
			continue;
		if (desc->physical_start != addr) {
			printf("    %-14s  %010llx  %10s  %010llx\n", "<gap>",
			       addr, "", desc->physical_start - addr);
		}
		size = desc->num_pages << EFI_PAGE_SHIFT;

		printf("%2d  %x:%-12s  %010llx  %010llx  %010llx  ", upto,
		       desc->type, efi_mem_type_name(desc->type),
		       desc->physical_start, desc->virtual_start, size);
		if (desc->attribute & EFI_MEMORY_RUNTIME)
			putc('r');
		printf("%llx", desc->attribute & ~EFI_MEMORY_RUNTIME);
		putc('\n');

		for (i = 0; i < attr_seen_count; i++) {
			if (attr_seen[i] == desc->attribute)
				break;
		}
		if (i == attr_seen_count && i < ATTR_SEEN_MAX)
			attr_seen[attr_seen_count++] = desc->attribute;
		addr = desc->physical_start + size;
	}

	printf("\nAttributes key:\n");
	for (i = 0; i < attr_seen_count; i++) {
		u64 attr = attr_seen[i];
		bool first;
		int j;

		printf("%c%llx: ", (attr & EFI_MEMORY_RUNTIME) ? 'r' : ' ',
		       attr & ~EFI_MEMORY_RUNTIME);
		for (j = 0, first = true; j < ARRAY_SIZE(mem_attr); j++) {
			if (attr & mem_attr[j].val) {
				if (first)
					first = false;
				else
					printf(", ");
				printf("%s", mem_attr[j].name);
			}
		}
		putc('\n');
	}
	if (skip_bs)
		printf("*Some areas are merged (use 'all' to see)\n");
}
