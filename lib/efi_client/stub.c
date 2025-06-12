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
#include <efi_stub.h>

/*
 * true if we must use the hardware UART directory (EFI not available). This
 * is normally false, meaning that character output is sent to the efi_putc()
 * routine. Once exit-boot-services is called, we must either not use character
 * output at all, or use a hardware UART directly, if there is a driver
 * available.
 */
bool use_hw_uart;

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

void *memcpy(void *dest, const void *src, size_t size)
{
	unsigned char *dptr = dest;
	const unsigned char *ptr = src;
	const unsigned char *end = src + size;

	while (ptr < end)
		*dptr++ = *ptr++;

	return dest;
}

void *memset(void *inptr, int ch, size_t size)
{
	char *ptr = inptr;
	char *end = ptr + size;

	while (ptr < end)
		*ptr++ = ch;

	return ptr;
}

int setup_info_table(struct efi_priv *priv, int size)
{
	struct efi_info_hdr *info;
	efi_status_t ret;

	/* Get some memory for our info table */
	priv->info_size = size;
	info = efi_malloc(priv, priv->info_size, &ret);
	if (ret) {
		printhex2(ret);
		puts(" No memory for info table: ");
		return ret;
	}

	memset(info, '\0', sizeof(*info));
	info->version = EFI_TABLE_VERSION;
	info->hdr_size = sizeof(*info);
	priv->info = info;
	priv->next_hdr = (char *)info + info->hdr_size;

	return 0;
}

/**
 * add_entry_addr() - Add a new entry to the efi_info list
 *
 * This adds an entry, consisting of a tag and two lots of data. This avoids the
 * caller having to coalesce the data first
 *
 * @priv: Pointer to our private information which contains the list
 * @type: Type of the entry to add
 * @ptr1: Pointer to first data block to add
 * @size1: Size of first data block in bytes (can be 0)
 * @ptr2: Pointer to second data block to add
 * @size2: Size of second data block in bytes (can be 0)
 */
void add_entry_addr(struct efi_priv *priv, enum efi_entry_t type, void *ptr1,
		    int size1, void *ptr2, int size2)
{
	struct efi_entry_hdr *hdr = priv->next_hdr;

	hdr->type = type;
	hdr->size = size1 + size2;
	hdr->addr = 0;
	hdr->link = ALIGN(sizeof(*hdr) + hdr->size, 16);
	priv->next_hdr += hdr->link;
	memcpy(hdr + 1, ptr1, size1);
	memcpy((void *)(hdr + 1) + size1, ptr2, size2);
	priv->info->total_size = (ulong)priv->next_hdr - (ulong)priv->info;
}
