/*
 * Copyright (c) 2015 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * EFI information obtained here:
 * http://wiki.phoenix.com/wiki/index.php/EFI_BOOT_SERVICES
 */

#include <common.h>
#include <debug_uart.h>
#include <errno.h>
#include <linux/err.h>
#include <linux/types.h>
#include <efi.h>

DECLARE_GLOBAL_DATA_PTR;

struct efi_priv {
	efi_handle_t parent_image;
	struct efi_device_path *device_path;
	struct efi_system_table *sys_table;
	struct efi_boot_services *boot;
	struct efi_runtime_services *run;
	bool use_pool_for_malloc;
	unsigned long ram_base;
};

static struct efi_priv *global_priv;

struct efi_system_table *efi_get_sys_table(void)
{
	return global_priv->sys_table;
}

unsigned long efi_get_ram_base(void)
{
	return global_priv->ram_base;
}

static efi_status_t setup_memory(struct efi_priv *priv)
{
	efi_guid_t loaded_image_guid = LOADED_IMAGE_PROTOCOL_GUID;
	struct efi_boot_services *boot = priv->boot;
	struct efi_loaded_image *loaded_image;
	efi_physical_addr_t addr;
	efi_status_t ret;
	int pages;
	void *buf;

	ret = boot->open_protocol(priv->parent_image, &loaded_image_guid,
				  (void **)&loaded_image, &priv->parent_image,
				  NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (ret) {
		debug("Cannot get loaded image protocol\n");
		return ret;
	}

	ret = boot->allocate_pool(loaded_image->image_data_type,
				  sizeof(struct global_data), &buf);
	global_data_ptr = buf;
	memset(gd, '\0', sizeof(*gd));
	if (ret)
		return ret;
	ret = boot->allocate_pool(loaded_image->image_data_type,
				  CONFIG_SYS_MALLOC_F_LEN, &buf);
	if (ret)
		return ret;
	gd->malloc_base = (ulong)buf;
	pages = CONFIG_EFI_RAM_SIZE >> 12;
	addr = 1ULL << 32;
	ret = boot->allocate_pages(EFI_ALLOCATE_MAX_ADDRESS,
				   loaded_image->image_data_type, pages, &addr);
	if (ret) {
		printf("(using pool %lx) ", ret);
		ret = boot->allocate_pool(loaded_image->image_data_type,
					  CONFIG_EFI_RAM_SIZE, &buf);
		if (ret)
			return ret;
		addr = (ulong)buf;
		priv->use_pool_for_malloc = true;
	}
	priv->ram_base = addr;
	gd->ram_size = pages << 12;

	return 0;
}

static void free_memory(struct efi_priv *priv)
{
	struct efi_boot_services *boot = priv->boot;

	if (priv->use_pool_for_malloc)
		boot->free_pool((void *)priv->ram_base);
	else
		boot->free_pages(priv->ram_base, gd->ram_size >> 12);

	boot->free_pool((void *)gd->malloc_base);
	boot->free_pool(gd);
	global_data_ptr = NULL;
}

/**
 * efi_main() - Start an EFI image
 *
 * This function is called by our EFI start-up code. It handles running
 * U-Boot. If it returns, EFI will continue. Another way to get back to EFI
 * is via reset_cpu().
 */
efi_status_t efi_main(efi_handle_t image, struct efi_system_table *sys_table)
{
	struct efi_priv local_priv, *priv = &local_priv;
	efi_status_t ret;

	/* Set up access to EFI data structures */
	memset(priv, '\0', sizeof(*priv));
	priv->sys_table = sys_table;
	priv->boot = sys_table->boottime;
	priv->parent_image = image;
	priv->run = sys_table->runtime;
	global_priv = priv;

	/* Set up the EFI debug UART so that printf() works */
	debug_uart_init();
	printf("U-Boot EFI ");

	ret = setup_memory(priv);
	if (ret) {
		printf("Failed to set up memory: ret=%lx\n", ret);
		return ret;
	}

	printf("starting\n");

	board_init_f(GD_FLG_SKIP_RELOC);
	board_init_r(NULL, 0);
	free_memory(priv);

	return EFI_SUCCESS;
}

void reset_cpu(ulong addr)
{
	struct efi_priv *priv = global_priv;

	free_memory(priv);
	printf("U-Boot EFI exiting\n");
	priv->boot->exit(priv->parent_image, EFI_SUCCESS, 0, NULL);
}
