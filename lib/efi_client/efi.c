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
#include <errno.h>
#include <malloc.h>
#include <linux/err.h>
#include <linux/types.h>
#include <efi.h>
#include <efi_api.h>

enum {
	/* magic number to trigger gdb breakpoint */
	GDB_MAGIC	= 0xdeadbeef,

	/* breakpoint address */
	GDB_ADDR	= 0x10000,
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

struct efi_runtime_services *efi_get_run(void)
{
	return global_priv->run;
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

/* helper for debug prints.. efi_free_pool() the result. */
uint16_t *efi_dp_str(struct efi_device_path *dp)
{
	struct efi_priv *priv = efi_get_priv();
	u16 *val;

	if (!priv->efi_dp_to_text)
		return NULL;

	val = priv->efi_dp_to_text->convert_device_path_to_text(dp, true,
								true);

	return val;
}
