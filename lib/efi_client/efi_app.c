// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2015 Google, Inc
 *
 * EFI information obtained here:
 * http://wiki.phoenix.com/wiki/index.php/EFI_BOOT_SERVICES
 *
 * This file implements U-Boot running as an EFI application.
 */

#define LOG_CATEGORY	LOGC_EFI

#include <cpu_func.h>
#include <debug_uart.h>
#include <dm.h>
#include <efi.h>
#include <efi_api.h>
#include <efi_stub.h>
#include <errno.h>
#include <fdt_simplefb.h>
#include <image.h>
#include <init.h>
#include <malloc.h>
#include <sysreset.h>
#include <u-boot/uuid.h>
#include <asm/global_data.h>
#include <linux/err.h>
#include <linux/types.h>
#include <asm/global_data.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/root.h>
#include <mapmem.h>
#include <linux/libfdt.h>
#include <fdt_support.h>

DECLARE_GLOBAL_DATA_PTR;

int copy_uboot_to_ram(void)
{
	return 0;
}

int do_elf_reloc_fixups(void)
{
	return 0;
}

int efi_init_obj_list(void)
{
	return EFI_SUCCESS;
}

int efi_info_get(enum efi_entry_t type, void **datap, int *sizep)
{
	return -ENOSYS;
}

int efi_get_mmap(struct efi_mem_desc **descp, int *sizep, uint *keyp,
		 int *desc_sizep, uint *versionp)
{
	struct efi_priv *priv = efi_get_priv();
	struct efi_boot_services *boot = priv->sys_table->boottime;
	efi_uintn_t size, desc_size, key;
	struct efi_mem_desc *desc;
	efi_status_t ret;
	u32 version;

	/* Get the memory map so we can switch off EFI */
	size = 0;
	ret = boot->get_memory_map(&size, NULL, &key, &desc_size, &version);
	if (ret != EFI_BUFFER_TOO_SMALL)
		return log_msg_ret("get", -ENOMEM);

	desc = malloc(size);
	if (!desc)
		return log_msg_ret("mem", -ENOMEM);

	ret = boot->get_memory_map(&size, desc, &key, &desc_size, &version);
	if (ret)
		return log_msg_ret("get", -EINVAL);

	*descp = desc;
	*sizep = size;
	*desc_sizep = desc_size;
	*versionp = version;
	*keyp = key;

	return 0;
}

static efi_status_t setup_memory(struct efi_priv *priv)
{
	struct efi_boot_services *boot = priv->boot;
	struct global_data *ptr;
	efi_physical_addr_t addr;
	efi_status_t ret;
	int pages;

	ptr = efi_malloc(priv, sizeof(*ptr), &ret);
	if (!ptr)
		return ret;
	memset(ptr, '\0', sizeof(*ptr));

	set_gd(ptr);

	gd->malloc_base = (ulong)efi_malloc(priv, CONFIG_VAL(SYS_MALLOC_F_LEN),
					    &ret);
	if (!gd->malloc_base)
		return ret;
	pages = CONFIG_EFI_RAM_SIZE >> 12;

	/*
	 * Try not to allocate any memory above 4GB, just for ease of looking at
	 * addresses.
	 */
	addr = 1ULL << 32;
	ret = boot->allocate_pages(EFI_ALLOCATE_MAX_ADDRESS,
				   priv->image_data_type, pages, &addr);
	if (ret) {
		log_info("(any address) ");
		ret = boot->allocate_pages(EFI_ALLOCATE_ANY_PAGES,
					   priv->image_data_type, pages, &addr);
	}
	if (ret) {
		log_info("(using pool %lx) ", ret);
		priv->ram_base = (ulong)efi_malloc(priv, CONFIG_EFI_RAM_SIZE,
						   &ret);
		if (!priv->ram_base)
			return ret;
		priv->use_pool_for_malloc = true;
	} else {
		log_info("(using allocated RAM address %lx) ", (ulong)addr);
		priv->ram_base = addr;
	}
	gd->ram_base = addr;
	gd->ram_size = pages << 12;

	return 0;
}

/**
 * free_memory() - Free memory used by the U-Boot app
 *
 * This frees memory allocated in setup_memory(), in preparation for returning
 * to UEFI. It also zeroes the global_data pointer.
 *
 * @priv: Private EFI data
 */
static void free_memory(struct efi_priv *priv)
{
	struct efi_boot_services *boot = priv->boot;

	if (priv->use_pool_for_malloc)
		efi_free(priv, (void *)priv->ram_base);
	else
		boot->free_pages(priv->ram_base,
				 gd->ram_size >> EFI_PAGE_SHIFT);

	efi_free(priv, (void *)gd->malloc_base);
	efi_free(priv, (void *)gd);
	set_gd((void *)NULL);
}

static void scan_tables(struct efi_system_table *sys_table)
{
	efi_guid_t acpi = EFI_ACPI_TABLE_GUID;
	efi_guid_t smbios = SMBIOS3_TABLE_GUID;
	uint i;

	for (i = 0; i < sys_table->nr_tables; i++) {
		struct efi_configuration_table *tab = &sys_table->tables[i];

		if (!memcmp(&tab->guid, &acpi, sizeof(efi_guid_t)))
			gd_set_acpi_start(map_to_sysmem(tab->table));
		else if (!memcmp(&tab->guid, &smbios, sizeof(efi_guid_t)))
			gd->arch.smbios_start = map_to_sysmem(tab->table);
	}
}

static void find_protocols(struct efi_priv *priv)
{
	efi_guid_t guid = EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID;
	struct efi_boot_services *boot = priv->boot;

	boot->locate_protocol(&guid, NULL, (void **)&priv->efi_dp_to_text);
}

static void efi_exit(void)
{
	struct efi_priv *priv = efi_get_priv();

	printf("U-Boot EFI exiting\n");
	priv->boot->exit(priv->parent_image, EFI_SUCCESS, 0, NULL);
}

/**
 * efi_main() - Start an EFI image
 *
 * This function is called by our EFI start-up code. It handles running
 * U-Boot. If it returns, EFI will continue. Another way to get back to EFI
 * is via reset_cpu().
 */
efi_status_t EFIAPI efi_main(efi_handle_t image,
			     struct efi_system_table *sys_table)
{
	struct efi_priv local_priv, *priv = &local_priv;
	efi_status_t ret;

	/* Set up access to EFI data structures */
	ret = efi_init(priv, "App", image, sys_table);
	if (ret) {
		printf("Failed to set up U-Boot: err=%lx\n", ret);
		return ret;
	}
	efi_set_priv(priv);

	/*
	 * Set up the EFI debug UART so that printf() works. This is
	 * implemented in the EFI serial driver, serial_efi.c. The application
	 * can use printf() freely.
	 */
	debug_uart_init();

	ret = setup_memory(priv);
	if (ret) {
		printf("Failed to set up memory: ret=%lx\n", ret);
		return ret;
	}

	scan_tables(priv->sys_table);
	find_protocols(priv);

	/*
	 * We could store the EFI memory map here, but it changes all the time,
	 * so this is only useful for debugging.
	 *
	 * ret = efi_store_memory_map(priv);
	 * if (ret)
	 *	return ret;
	 */

	printf("starting\n");

	board_init_f(GD_FLG_SKIP_RELOC);
	printf("&gd %p gd %p new_gd %p\n", &global_data_ptr, gd, gd->new_gd);
	gd = gd->new_gd;
	// efi_exit();
	board_init_r(NULL, 0);
	free_memory(priv);
	efi_exit();

	return EFI_SUCCESS;
}

static int efi_sysreset_request(struct udevice *dev, enum sysreset_t type)
{
	efi_exit();

	return -EINPROGRESS;
}

/*
 * Attempt to relocate the kernel to somewhere the firmware isn't using
 */
int board_fixup_os(void *ctx, struct event *evt)
{
	int pages;
	u64 addr;
	efi_status_t status;
	struct efi_priv *priv = efi_get_priv();
	struct efi_boot_services *boot = priv->boot;
	struct event_os_load *os_load = &evt->data.os_load;

	pages = DIV_ROUND_UP(os_load->size, EFI_PAGE_SIZE);

	addr = os_load->addr;

	/* Try to allocate at the preferred address */
	status = boot->allocate_pages(EFI_ALLOCATE_ADDRESS, EFI_LOADER_DATA,
				      pages, &addr);
	if (!status)
		return 0;

	/* That failed, so try allocating anywhere there's enough room */
	status = boot->allocate_pages(EFI_ALLOCATE_ANY_PAGES, EFI_LOADER_DATA, pages, &addr);
	if (status) {
		printf("Failed to alloc %lx bytes: %lx\n", os_load->size,
		       status);
		return -EFAULT;
	}

	/* Make sure bootm knows where we loaded the image */
	os_load->addr = addr;

	return 0;
}
EVENT_SPY_FULL(EVT_BOOT_OS_ADDR, board_fixup_os);

int efi_app_exit_boot_services(struct efi_priv *priv, uint key)
{
	const struct efi_boot_services *boot = priv->boot;
	int ret;

	ret = boot->exit_boot_services(priv->parent_image, key);
	if (ret)
		return ret;

	return 0;
}

/**
 * is_efi_memory_reserved() - Check if EFI memory type should be preserved
 *
 * @type: EFI memory type
 * Return: true if memory type should be preserved, false otherwise
 */
static bool is_efi_memory_reserved(u32 type)
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
 * is_region_in_dt_reserved_memory() - Check if memory region is covered by DT reserved-memory
 *
 * @fdt: Device tree blob
 * @start: Start address of region to check
 * @end: End address of region to check
 * Return: true if region overlaps with any reserved-memory node, false otherwise
 */
static bool is_region_in_dt_reserved_memory(void *fdt, u64 start, u64 end)
{
	int reserved_mem_node = fdt_path_offset(fdt, "/reserved-memory");
	if (reserved_mem_node < 0)
		return false;

	int child;
	fdt_for_each_subnode(child, fdt, reserved_mem_node) {
		const fdt32_t *reg;
		int len;
		
		reg = fdt_getprop(fdt, child, "reg", &len);
		if (!reg || len < 8)
			continue;
		
		/* Parse reg property - assuming #address-cells=2, #size-cells=2 */
		u64 rsv_start = fdt64_to_cpu(*(fdt64_t *)reg);
		u64 rsv_size = fdt64_to_cpu(*((fdt64_t *)reg + 1));
		u64 rsv_end = rsv_start + rsv_size - 1;

		/* Check for overlap */
		if (!(end < rsv_start || start > rsv_end))
			return true;
	}

	return false;
}

/**
 * add_efi_region_to_dt() - Add EFI reserved region to device tree reserved-memory
 *
 * @fdt: Device tree blob
 * @start: Start address of region
 * @size: Size of region  
 * @type_name: EFI memory type name for node naming
 * Return: 0 on success, negative error code on failure
 */
static int add_efi_region_to_dt(void *fdt, u64 start, u64 size, const char *type_name)
{
	int reserved_mem_node, new_node;
	char node_name[64];
	fdt32_t reg_prop[4];
	int ret;

	/* Find or create /reserved-memory node */
	reserved_mem_node = fdt_path_offset(fdt, "/reserved-memory");
	if (reserved_mem_node < 0) {
		/* Create /reserved-memory node */
		reserved_mem_node = fdt_add_subnode(fdt, 0, "reserved-memory");
		if (reserved_mem_node < 0) {
			printf("Failed to create /reserved-memory node: %s\n", 
			       fdt_strerror(reserved_mem_node));
			return reserved_mem_node;
		}

		/* Set required properties for reserved-memory */
		fdt32_t addr_cells = cpu_to_fdt32(2);
		fdt32_t size_cells = cpu_to_fdt32(2);
		
		ret = fdt_setprop(fdt, reserved_mem_node, "#address-cells", &addr_cells, sizeof(addr_cells));
		if (ret < 0) return ret;
		
		ret = fdt_setprop(fdt, reserved_mem_node, "#size-cells", &size_cells, sizeof(size_cells));
		if (ret < 0) return ret;
		
		ret = fdt_setprop(fdt, reserved_mem_node, "ranges", NULL, 0);
		if (ret < 0) return ret;
	}

	/* Create node name based on type and address */
	snprintf(node_name, sizeof(node_name), "efi-%s@%llx", type_name, start);
	
	/* Convert spaces and underscores to hyphens for a valid node name */
	for (char *p = node_name; *p; p++) {
		if (*p == ' ' || *p == '_')
			*p = '-';
	}

	/* Add new subnode */
	new_node = fdt_add_subnode(fdt, reserved_mem_node, node_name);
	if (new_node < 0) {
		printf("Failed to create node %s: %s\n", node_name, fdt_strerror(new_node));
		return new_node;
	}

	/* Set reg property - #address-cells=2, #size-cells=2 */
	reg_prop[0] = cpu_to_fdt32(start >> 32);
	reg_prop[1] = cpu_to_fdt32(start & 0xffffffff);
	reg_prop[2] = cpu_to_fdt32(size >> 32);
	reg_prop[3] = cpu_to_fdt32(size & 0xffffffff);

	ret = fdt_setprop(fdt, new_node, "reg", reg_prop, sizeof(reg_prop));
	if (ret < 0) {
		printf("Failed to set reg property: %s\n", fdt_strerror(ret));
		return ret;
	}

	/* Add no-map property to prevent Linux from using this memory */
	ret = fdt_setprop(fdt, new_node, "no-map", NULL, 0);
	if (ret < 0) {
		printf("Failed to set no-map property: %s\n", fdt_strerror(ret));
		return ret;
	}

	printf("  -> Added reserved-memory node: %s (0x%llx - 0x%llx)\n", 
	       node_name, start, start + size - 1);

	return 0;
}

/**
 * sync_efi_reserved_regions_to_dt() - Print EFI reserved regions and add missing ones to DT
 *
 * @fdt: Device tree blob
 * Return: true if any uncovered regions found, false otherwise
 */
static bool sync_efi_reserved_regions_to_dt(void *fdt)
{
	struct efi_mem_desc *map, *desc, *end;
	int desc_size, size, upto;
	uint version, key;
	int ret;
	bool found_unreported = false;

	/* Get the EFI memory map */
	ret = efi_get_mmap(&map, &size, &key, &desc_size, &version);
	if (ret) {
		printf("Failed to get EFI memory map: %d\n", ret);
		return false;
	}

	printf("EFI Memory Map Analysis:\n");
	printf("%-4s %-18s %-18s %-18s %s\n", "ID", "Type", "Start", "End", "In DT?");
	printf("------------------------------------------------------------------------\n");

	end = (void *)map + size;
	for (upto = 0, desc = map; desc < end;
	     desc = efi_get_next_mem_desc(desc, desc_size), upto++) {
		u64 start = desc->physical_start;
		u64 end_addr = start + (desc->num_pages << EFI_PAGE_SHIFT) - 1;
		u64 region_size = desc->num_pages << EFI_PAGE_SHIFT;

		if (!is_efi_memory_reserved(desc->type))
			continue;

		bool in_dt_reserved = is_region_in_dt_reserved_memory(fdt, start, end_addr);

		/* Print the region */
		printf("%-4d %-18s 0x%-16llx 0x%-16llx %s",
		       upto,
		       efi_mem_type_name(desc->type),
		       start,
		       end_addr,
		       in_dt_reserved ? "YES" : "NO");

		if (!in_dt_reserved) {
			found_unreported = true;
			printf(" -> ADDING\n");
			
			/* Add this region to device tree */
			const char *type_name = efi_mem_type_name(desc->type);
			int ret = add_efi_region_to_dt(fdt, start, region_size, type_name);
			if (ret < 0) {
				printf("  -> Failed to add region: %s\n", fdt_strerror(ret));
			}
		} else {
			printf("\n");
		}
	}

	free(map);
	return found_unreported;
}

/**
 * print_dt_reserved_memory_regions() - Print all device tree reserved-memory nodes
 *
 * @fdt: Device tree blob
 */
static void print_dt_reserved_memory_regions(void *fdt)
{
	printf("\nDevice Tree Reserved-Memory Regions:\n");
	printf("%-4s %-20s %-18s %-18s\n", "ID", "Name", "Start", "Size");
	printf("----------------------------------------------------------------\n");

	int reserved_mem_node = fdt_path_offset(fdt, "/reserved-memory");
	if (reserved_mem_node < 0) {
		printf("No /reserved-memory node found in device tree\n");
		return;
	}

	int child, id = 0;
	fdt_for_each_subnode(child, fdt, reserved_mem_node) {
		const char *name = fdt_get_name(fdt, child, NULL);
		const fdt32_t *reg;
		int len;
		
		reg = fdt_getprop(fdt, child, "reg", &len);
		if (reg && len >= 8) {
			/* Parse reg property - assuming #address-cells=2, #size-cells=2 */
			u64 rsv_start = fdt64_to_cpu(*(fdt64_t *)reg);
			u64 rsv_size = fdt64_to_cpu(*((fdt64_t *)reg + 1));
			printf("%-4d %-20s 0x%-16llx 0x%-16llx\n", 
			       id++, name ? name : "(unnamed)", rsv_start, rsv_size);
		} else {
			printf("%-4d %-20s %-18s %-18s\n", 
			       id++, name ? name : "(unnamed)", "(no reg)", "(no reg)");
		}
	}

	if (id == 0)
		printf("No reserved-memory regions found\n");
}

/**
 * compare_efi_dt_memory_reservations() - Compare EFI memory map with DT reserved-memory nodes
 *
 * This function compares the EFI memory map with the device tree's reserved-memory
 * nodes and prints out regions that are reserved in EFI but not mentioned in the
 * device tree's /reserved-memory node. This helps identify memory regions that
 * EFI considers reserved but which Linux might try to use.
 *
 * @fdt: Pointer to the device tree blob
 */
static void compare_efi_dt_memory_reservations(void *fdt)
{
	printf("=== Comparing EFI Memory Map with Device Tree Reserved Regions ===\n");

	bool found_unreported = sync_efi_reserved_regions_to_dt(fdt);

	if (found_unreported) {
		printf("\n✓ Added missing EFI reserved regions to device tree.\n");
	} else {
		printf("\n✓ All EFI reserved regions were already covered by device tree reservations.\n");
	}

	print_dt_reserved_memory_regions(fdt);

	printf("=== End Memory Comparison ===\n\n");
}

int ft_system_setup(void *fdt, struct bd_info *bd)
{
	struct efi_mem_desc *map, *desc, *end;
	u64 ram_start, ram_end;
	int desc_size;
	int ret, upto;
	uint version;
	int size;
	uint key;

	ret = efi_get_mmap(&map, &size, &key, &desc_size, &version);
	if (ret)
		return log_msg_ret("erm", ret);

	if (_DEBUG)
		efi_dump_mem_table(map, size, desc_size, false);

	ram_start = -1ULL;
	ram_end = -1ULL;
	end = (void *)map + size;
	for (upto = 0, desc = map; desc < end;
	     desc = efi_get_next_mem_desc(desc, desc_size), upto++) {
		u64 base = desc->physical_start, limit;

		if (!efi_mem_is_boot_services(desc->type) &&
		    desc->type != EFI_CONVENTIONAL_MEMORY)
			continue;

		if (ram_start == -1ULL)
			ram_start = base;
		limit = base + (desc->num_pages << EFI_PAGE_SHIFT);
		log_debug("%d: %s: %llx limit %llx\n", upto,
			  efi_mem_type_name(desc->type), base, limit);
		if (ram_end == -1ULL || limit > ram_end)
			ram_end = limit;
	}

	ram_start = 0x80000000;
	ram_end = ram_start + 3 * SZ_4G;
	// ram_end = ram_start + 1 * SZ_4G;
	// ram_start = SZ_4G;
	// ram_end = 4 * SZ_4G;

	log_info("RAM extends from %llx to %llx\n", ram_start, ram_end);
	ret = fdt_fixup_memory(fdt, ram_start, ram_end - ram_start);
	if (ret) {
		printf("failed fixup memory\n");
		return ret;
	}

	if (IS_ENABLED(CONFIG_FDT_SIMPLEFB)) {
		ret = fdt_simplefb_add_node(fdt);
		if (ret)
			log_warning("failed to set up simplefb\n");
	}

	/* Compare EFI memory map with device tree reserved regions */
	ret = efi_mem_reserved_sync(fdt, true);
	if (ret)
		log_warning("failed to set up reserved memory\n");

	free(map);

	return 0;
}

static const struct udevice_id efi_sysreset_ids[] = {
	{ .compatible = "efi,reset" },
	{ }
};

static struct sysreset_ops efi_sysreset_ops = {
	.request = efi_sysreset_request,
};

U_BOOT_DRIVER(efi_sysreset) = {
	.name = "efi-sysreset",
	.id = UCLASS_SYSRESET,
	.of_match = efi_sysreset_ids,
	.ops = &efi_sysreset_ops,
};
