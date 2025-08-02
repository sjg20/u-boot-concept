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
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

enum {
	/* magic number to trigger gdb breakpoint */
	GDB_MAGIC	= 0xdeadbeef,

	/* breakpoint address */
	GDB_ADDR	= 0x10000,

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

/**
 * struct gdb_marker - structure to simplify debugging with gdb
 *
 * This struct is placed in memory and accessed to trigger a breakpoint in
 * gdb.
 *
 * @magic: Magic number (GDB_MAGIC)
 * @base: Base address of the app
 */
struct gdb_marker {
	union {
		u32 magic;
		u64 space;
	};
	void *base;
};

static struct efi_priv *global_priv;

struct efi_priv *efi_get_priv(void)
{
	return global_priv;
}

void efi_set_priv(struct efi_priv *priv)
{
	global_priv = priv;
}

struct efi_system_table *efi_get_sys_table(void)
{
	return global_priv->sys_table;
}

struct efi_boot_services *efi_get_boot(void)
{
	return global_priv->boot;
}

efi_handle_t efi_get_parent_image(void)
{
	return global_priv->parent_image;
}

unsigned long efi_get_ram_base(void)
{
	return global_priv->ram_base;
}

/*
 * Global declaration of gd.
 *
 * As we write to it before relocation we have to make sure it is not put into
 * a .bss section which may overlap a .rela section. Initialization forces it
 * into a .data section which cannot overlap any .rela section.
 */
struct global_data *global_data_ptr = (struct global_data *)~0;

/*
 * Unfortunately we cannot access any code outside what is built especially
 * for the stub. lib/string.c is already being built for the U-Boot payload
 * so it uses the wrong compiler flags. Add our own memset() here.
 */
static void efi_memset(void *ptr, int ch, int size)
{
	char *dest = ptr;

	while (size-- > 0)
		*dest++ = ch;
}

/*
 * Since the EFI stub cannot access most of the U-Boot code, add our own
 * simple console output functions here. The EFI app will not use these since
 * it can use the normal console.
 */
void efi_putc(struct efi_priv *priv, const char ch)
{
	struct efi_simple_text_output_protocol *con = priv->sys_table->con_out;
	uint16_t ucode[2];

	ucode[0] = ch;
	ucode[1] = '\0';
	con->output_string(con, ucode);
}

void efi_puts(struct efi_priv *priv, const char *str)
{
	while (*str)
		efi_putc(priv, *str++);
}

int efi_init(struct efi_priv *priv, const char *banner, efi_handle_t image,
	     struct efi_system_table *sys_table)
{
	efi_guid_t loaded_image_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
	struct efi_boot_services *boot = sys_table->boottime;
	struct efi_loaded_image *loaded_image;
	int ret;

	efi_memset(priv, '\0', sizeof(*priv));
	priv->sys_table = sys_table;
	priv->boot = sys_table->boottime;
	priv->parent_image = image;
	priv->run = sys_table->runtime;

	efi_puts(priv, "U-Boot EFI ");
	efi_puts(priv, banner);
	efi_putc(priv, ' ');

	ret = boot->open_protocol(priv->parent_image, &loaded_image_guid,
				  (void **)&loaded_image, priv->parent_image,
				  NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (ret) {
		efi_puts(priv, "Failed to get loaded image protocol\n");
		return ret;
	}
	priv->loaded_image = loaded_image;
	priv->image_data_type = loaded_image->image_data_type;

	if (IS_ENABLED(CONFIG_EFI_APP_DEBUG)) {
		struct gdb_marker *marker = (struct gdb_marker *)GDB_ADDR;
		char buf[64];

		marker->base = priv->loaded_image->image_base;
		snprintf(buf, sizeof(buf), "\ngdb marker at %p base %p\n",
			 marker, marker->base);
		efi_puts(priv, buf);
		marker->magic = 0xdeadbeef;
	}

	return 0;
}

void *efi_malloc(struct efi_priv *priv, int size, efi_status_t *retp)
{
	struct efi_boot_services *boot = priv->boot;
	void *buf = NULL;

	*retp = boot->allocate_pool(priv->image_data_type, size, &buf);

	return buf;
}

void efi_free(struct efi_priv *priv, void *ptr)
{
	struct efi_boot_services *boot = priv->boot;

	boot->free_pool(ptr);
}

void *efi_alloc(size_t size)
{
	struct efi_priv *priv = efi_get_priv();
	efi_status_t ret;

	return efi_malloc(priv, size, &ret);
}

void efi_free_pool(void *ptr)
{
	struct efi_priv *priv = efi_get_priv();

	efi_free(priv, ptr);
}

void efi_print_mem_table(struct efi_mem_desc *desc, int size, int desc_size,
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
		const char *name;
		u64 size;

		if (skip_bs && efi_mem_is_boot_services(desc->type))
			continue;
		if (desc->physical_start != addr) {
			printf("    %-14s  %010llx  %10s  %010llx\n", "<gap>",
			       addr, "", desc->physical_start - addr);
		}
		size = desc->num_pages << EFI_PAGE_SHIFT;

		name = desc->type < ARRAY_SIZE(type_name) ?
				type_name[desc->type] : "<invalid>";
		printf("%2d  %x:%-12s  %010llx  %010llx  %010llx  ", upto,
		       desc->type, name, desc->physical_start,
		       desc->virtual_start, size);
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

static bool efi_mem_type_is_usable(u32 type)
{
	switch (type) {
	case EFI_CONVENTIONAL_MEMORY:
	case EFI_LOADER_DATA:
	case EFI_LOADER_CODE:
	case EFI_BOOT_SERVICES_CODE:
	case EFI_BOOT_SERVICES_DATA:
	case EFI_ACPI_RECLAIM_MEMORY:
	case EFI_RUNTIME_SERVICES_CODE:
	case EFI_RUNTIME_SERVICES_DATA:
		return true;
	case EFI_RESERVED_MEMORY_TYPE:
	case EFI_UNUSABLE_MEMORY:
	case EFI_UNACCEPTED_MEMORY_TYPE:
	case EFI_MMAP_IO:
	case EFI_MMAP_IO_PORT:
	case EFI_PERSISTENT_MEMORY_TYPE:
	default:
		return false;
	}
}

int dram_init_banksize_from_memmap(struct efi_mem_desc *desc, int size,
				   int desc_size)
{
	struct efi_mem_desc *end;
	int bank = 0;
	int num_banks;
	ulong start, max_addr = 0;
	bool first = true;

	end = (struct efi_mem_desc *)((ulong)desc + size);
	for (num_banks = 0;
	     desc < end && num_banks < CONFIG_NR_DRAM_BANKS;
	     desc = efi_get_next_mem_desc(desc, desc_size)) {
		/*
		 * We only use conventional memory and ignore
		 * anything less than 1MB.
		 */
		log_info("EFI bank #%d: start %llx, size %llx type %u\n",
			 bank, desc->physical_start,
			 desc->num_pages << EFI_PAGE_SHIFT, desc->type);
		if (!efi_mem_type_is_usable(desc->type)) {
			printf("not usable\n");
			continue;
		}
		if (first) {
			start = desc->physical_start;
			first = false;
		}
		max_addr = max((u64)max_addr, desc->physical_start +
				 (desc->num_pages << EFI_PAGE_SHIFT));
	}

	gd->bd->bi_dram[num_banks].start = start;
	gd->bd->bi_dram[num_banks].size = max_addr - start;
	num_banks++;

	return num_banks;
}
