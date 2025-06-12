// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2015 Google, Inc
 * Copyright (c) 2024 Linaro, Ltd.
 *
 * EFI information obtained here:
 * http://wiki.phoenix.com/wiki/index.php/EFI_BOOT_SERVICES
 *
 * Call ExitBootServices() and launch U-Boot from an EFI environment.
 */

#include <debug_uart.h>
#include <efi.h>
#include <efi_api.h>
#include <efi_stub.h>
#include <malloc.h>
#include <asm/io.h>
#include <linux/err.h>
#include <linux/types.h>

static bool ebs_called;

void _debug_uart_putc(int ch)
{
	struct efi_priv *priv = efi_get_priv();

	if (ch == '\n')
		_debug_uart_putc('\r');
	/*
	 * After calling EBS we can't log anywhere.
	 * NOTE: for development it is possible to re-implement
	 * your boards debug uart here like in efi_stub.c for x86.
	 */
	if (!ebs_called)
		efi_putc(priv, ch);
}

void _debug_uart_init(void) {}

DEBUG_UART_FUNCS;

void putc(const char ch)
{
	_debug_uart_putc(ch);
}

void puts(const char *str)
{
	while (*str)
		putc(*str++);
}

/**
 * efi_main() - Start an EFI image
 *
 * This function is called by our EFI start-up code. It handles running
 * U-Boot. If it returns, EFI will continue.
 */
efi_status_t EFIAPI efi_main(efi_handle_t image,
			     struct efi_system_table *sys_table)
{
	struct efi_priv local_priv, *priv = &local_priv;
	struct efi_boot_services *boot = sys_table->boottime;
	struct efi_entry_memmap map;
	struct efi_gop *gop;
	struct efi_entry_gopmode mode;
	struct efi_entry_systable table;
	efi_guid_t efi_gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	efi_status_t ret;

	ebs_called = false;

	ret = efi_init(priv, "Payload", image, sys_table);
	if (ret) {
		printhex2(ret);
		puts(" efi_init() failed\n");
		return ret;
	}
	efi_set_priv(priv);

	phys_addr_t reloc_addr = ULONG_MAX;
	ret = boot->allocate_pages(EFI_ALLOCATE_MAX_ADDRESS, EFI_LOADER_CODE,
				   (phys_addr_t)_binary_u_boot_bin_size / EFI_PAGE_SIZE,
				   &reloc_addr);
	if (ret != EFI_SUCCESS) {
		puts("Failed to allocate memory for U-Boot: ");
		printhex2(ret);
		putc('\n');
		return ret;
	}

	ret = efi_store_memory_map(priv);
	if (ret)
		return ret;

	ret = setup_info_table(priv, priv->memmap_size + 128);
	if (ret)
		return ret;

	ret = boot->locate_protocol(&efi_gop_guid, NULL, (void **)&gop);
	if (ret) {
		puts(" GOP unavailable\n");
	} else {
		mode.fb_base = gop->mode->fb_base;
		mode.fb_size = gop->mode->fb_size;
		mode.info_size = gop->mode->info_size;
		add_entry_addr(priv, EFIET_GOP_MODE, &mode, sizeof(mode),
			       gop->mode->info,
			       sizeof(struct efi_gop_mode_info));
	}

	table.sys_table = (ulong)sys_table;
	add_entry_addr(priv, EFIET_SYS_TABLE, &table, sizeof(table), NULL, 0);

	ret = efi_stub_exit_boot_services();
	if (ret)
		return ret;

	/* The EFI console won't work now :( */
	ebs_called = true;

	map.version = priv->memmap_version;
	map.desc_size = priv->memmap_desc_size;
	add_entry_addr(priv, EFIET_MEMORY_MAP, &map, sizeof(map),
		       priv->memmap_desc, priv->memmap_size);
	add_entry_addr(priv, EFIET_END, NULL, 0, 0, 0);

	memcpy((void *)reloc_addr, _binary_u_boot_bin_start,
	       (ulong)_binary_u_boot_bin_end -
	       (ulong)_binary_u_boot_bin_start);

/* This will only work if you patched your own debug uart into this file. */
#ifdef DEBUG
	puts("EFI table at ");
	printhex8((ulong)priv->info);
	puts(" size ");
	printhex8(priv->info->total_size);
	putc('\n');
#endif
	typedef void (*func_t)(u64 x0, u64 x1, u64 x2, u64 x3);

	puts("Jumping to U-Boot\n");
	((func_t)reloc_addr)((phys_addr_t)priv->info, 0, 0, 0);

	return EFI_LOAD_ERROR;
}
