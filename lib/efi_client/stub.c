// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2015 Google, Inc
 *
 * EFI information obtained here:
 * http://wiki.phoenix.com/wiki/index.php/EFI_BOOT_SERVICES
 *
 * Provides helper functions for use with the stub
 */

#include <debug_uart.h>
#include <efi.h>
#include <efi_api.h>

int efi_stub_exit_boot_services(void)
{
	struct efi_priv *priv = efi_get_priv();
	const struct efi_boot_services *boot = priv->boot;
	efi_uintn_t size;
	u32 version;
	efi_status_t ret;

	size = priv->memmap_alloc;
	ret = boot->get_memory_map(&size, priv->memmap_desc,
				   &priv->memmap_key,
				   &priv->memmap_desc_size, &version);
	if (ret) {
		printhex2(ret);
		puts(" Can't get memory map\n");
		return ret;
	}
	ret = boot->exit_boot_services(priv->parent_image, priv->memmap_key);
	if (ret)
		return ret;

	return 0;
}
