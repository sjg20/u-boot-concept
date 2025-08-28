// SPDX-License-Identifier: GPL-2.0+
/*
 * Syncing EFI memory-map to devicetree
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY	LOGC_EFI

#include <efi.h>
#include <efi_api.h>
#include <fdt_support.h>
#include <mapmem.h>
#include <linux/err.h>
#include <linux/libfdt.h>
#include <linux/types.h>

/**
 * is_reserved() - Check if EFI memory type should be preserved
 *
 * @type: EFI memory type
 * Return: true if memory type should be preserved, false otherwise
 */
static bool is_reserved(u32 type)
{
	switch (type) {
	case EFI_RESERVED_MEMORY_TYPE:
	case EFI_RUNTIME_SERVICES_CODE:
	case EFI_RUNTIME_SERVICES_DATA:
	case EFI_UNUSABLE_MEMORY:
	case EFI_ACPI_RECLAIM_MEMORY:
	case EFI_ACPI_MEMORY_NVS:
		return true;
	default:
		return false;
	}
}

/**
 * dt_region_exists() - Check if memory region is covered by DT reserved-memory
 *
 * @fdt: Device tree blob
 * @start: Start address of region to check
 * @end: End address of region to check
 * Return: true if region overlaps with any reserved-memory node, else false
 */
static bool dt_region_exists(void *fdt, u64 start, u64 end)
{
	int node, reserved;

	reserved = fdt_path_offset(fdt, "/reserved-memory");
	if (reserved < 0)
		return false;

	fdt_for_each_subnode(node, fdt, reserved) {
		const fdt32_t *reg;
		u64 start, size, end;
		int len;

		reg = fdt_getprop(fdt, node, "reg", &len);
		if (!reg || len < sizeof(u32) * 2)
			continue;

		/* Parse reg property - assuming #address-cells=2, #size-cells=2 */
		start = fdt64_to_cpu(*(fdt64_t *)reg);
		size = fdt64_to_cpu(*((fdt64_t *)reg + 1));
		end = start + size - 1;

		/* Check for overlap */
		if (!(end < start || start > end))
			return true;
	}

	return false;
}

/**
 * dt_add_reserved() - Add EFI reserved region to device tree reserved-memory
 *
 * @fdt: Device tree blob
 * @start: Start address of region
 * @size: Size of region
 * @type_name: EFI memory type name for node naming
 * Return: 0 on success, negative error code on failure
 */
static int dt_add_reserved(void *fdt, u64 start, u64 size,
			   const char *type_name)
{
	int reserved, node;
	char node_name[64];
	fdt32_t reg_prop[4];
	char *p;
	int ret;

	/* Find or create /reserved-memory node */
	reserved = fdt_path_offset(fdt, "/reserved-memory");
	if (reserved < 0) {
		/* Create /reserved-memory node */
		reserved = fdt_add_subnode(fdt, 0, "reserved-memory");
		if (reserved < 0) {
			printf("Failed to create /reserved-memory node: %s\n",
			       fdt_strerror(reserved));
			return reserved;
		}

		ret = fdt_setprop_u64(fdt, reserved, "#address-cells", 2);
		if (ret)
			return ret;

		ret = fdt_setprop_u64(fdt, reserved, "#size-cells", 2);
		if (ret)
			return ret;

		ret = fdt_setprop(fdt, reserved, "ranges", NULL, 0);
		if (ret)
			return ret;
	}

	/* Create node name based on type and address */
	snprintf(node_name, sizeof(node_name), "efi-%s@%llx", type_name, start);

	/* Convert spaces and underscores to hyphens for a valid node name */
	for (p = node_name; *p; p++) {
		if (*p == ' ' || *p == '_')
			*p = '-';
	}

	/* Add new subnode */
	node = fdt_add_subnode(fdt, reserved, node_name);
	if (node < 0) {
		printf("Failed to create node %s: %s\n", node_name,
		       fdt_strerror(node));
		return node;
	}

	/* Set reg property - #address-cells=2, #size-cells=2 */
	reg_prop[0] = cpu_to_fdt32(start >> 32);
	reg_prop[1] = cpu_to_fdt32(start & 0xffffffff);
	reg_prop[2] = cpu_to_fdt32(size >> 32);
	reg_prop[3] = cpu_to_fdt32(size & 0xffffffff);

	ret = fdt_setprop(fdt, node, "reg", reg_prop, sizeof(reg_prop));
	if (ret) {
		printf("Failed to set reg property: %s\n", fdt_strerror(ret));
		return ret;
	}

	/* Add no-map property to prevent Linux from using this memory */
	ret = fdt_setprop(fdt, node, "no-map", NULL, 0);
	if (ret) {
		printf("Failed to set no-map property: %s\n",
		       fdt_strerror(ret));
		return ret;
	}

	printf("added reserved-memory node: %s (0x%llx - 0x%llx)\n",
	       node_name, start, start + size - 1);

	return 0;
}

/**
 * sync_to_dt() - Print EFI reserved regions and add missing ones to DT
 *
 * @fdt: Device tree blob
 * Return: true if any uncovered regions found, false otherwise
 */
static int sync_to_dt(void *fdt, bool verbose)
{
	struct efi_mem_desc *map, *desc, *end;
	int desc_size, size, upto;
	uint version, key;
	int synced = 0;
	int ret;

	/* Get the EFI memory map */
	ret = efi_get_mmap(&map, &size, &key, &desc_size, &version);
	if (ret) {
		printf("Failed to get EFI memory map: %d\n", ret);
		return ret;
	}

	if (verbose) {
		printf("EFI Memory Map Analysis:\n");
		printf("%-4s %-18s %-16s %-16s %s\n", "ID", "Type", "Start",
		       "End", "In DT?");
		printf("-------------------------------------------------------"
		       "-----------------\n");
	}

	end = (void *)map + size;
	for (upto = 0, desc = map; desc < end;
	     desc = efi_get_next_mem_desc(desc, desc_size), upto++) {
		u64 start = desc->physical_start;
		u64 end_addr = start + (desc->num_pages << EFI_PAGE_SHIFT) - 1;
		u64 region_size = desc->num_pages << EFI_PAGE_SHIFT;
		bool present;

		if (!is_reserved(desc->type))
			continue;

		present = dt_region_exists(fdt, start, end_addr);

		/* Print the region */
		if (verbose) {
			printf("%-4d %-18s %-16llx %-16llx %s", upto,
			       efi_mem_type_name(desc->type), start, end_addr,
			       present ? "yes" : "no");
		}

		if (!present) {
			const char *type_name;
			int ret;

			if (verbose)
				printf(" -> adding\n");

			/* Add this region to device tree */
			type_name = efi_mem_type_name(desc->type);
			ret = dt_add_reserved(fdt, start, region_size,
					      type_name);
			if (ret) {
				printf("Failed to add region: %s\n",
				       fdt_strerror(ret));
				free(map);
				return ret;
			}
			synced++;
		} else if (verbose) {
			printf("\n");
		}
	}
	free(map);

	return synced;
}

int efi_mem_reserved_sync(void *fdt, bool verbose)
{
	int synced;

	if (verbose)
		printf("Comparing EFI memory-map with reserved-memory\n");

	synced = sync_to_dt(fdt, verbose);
	if (synced < 0) {
		printf("Failed to sync EFI reserved regions: error %d\n",
		       synced);
		return synced;
	}

	if (verbose) {
		printf("Regions added: %d\n", synced);
		fdt_print_reserved(fdt);
	}

	return synced;
}
