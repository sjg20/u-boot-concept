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
#include <asm/sections.h>

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
	// if (!use_hw_uart)
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

efi_status_t arch_efi_main_init(struct efi_priv *priv,
				struct efi_boot_services *boot)
{
	phys_addr_t reloc_addr = ULONG_MAX;
	int ret;

	printascii("start");
	printhex8((ulong)_start);
	printch(' ');

	printhex8((ulong)arch_efi_main_init);
	printch(' ');
	printhex8((ulong)&reloc_addr);
	printch(' ');
	printhex8((ulong)&_binary_u_boot_bin_size);
	ret = boot->allocate_pages(EFI_ALLOCATE_MAX_ADDRESS, EFI_LOADER_CODE,
				   (phys_addr_t)_binary_u_boot_bin_size / EFI_PAGE_SIZE,
				   &reloc_addr);
	if (ret != EFI_SUCCESS) {
		puts("Failed to allocate memory for U-Boot: ");
		printhex2(ret);
		putc('\n');
		return ret;
	}
	priv->jump_addr = reloc_addr;

	return 0;
}

void arch_efi_jump_to_payload(struct efi_priv *priv)
{
	typedef void (*func_t)(u64 x0, u64 x1, u64 x2, u64 x3);

	((func_t)priv->jump_addr)((phys_addr_t)priv->info, 0, 0, 0);
}
