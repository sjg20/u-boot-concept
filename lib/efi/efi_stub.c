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

struct efi_system_table *global_sys_table;
extern char __u_boot_bin_begin[], __u_boot_bin_end[];

static void copy_uboot(ulong dest, ulong src, ulong src_end)
{
	unsigned long *dptr = (void *)dest;
	unsigned long *ptr = (void *)src;
	unsigned long *end = (void *)src_end;

	while (ptr < end)
		*dptr++ = *ptr++;
}

static void jump_to_uboot(ulong addr)
{
	typedef void (*func_t)(void);

#ifdef CONFIG_EFI_STUB_32BIT
	((func_t)addr)();
#else
	/* TODO(sjg@chromium.org): Write this! */
// 	cpu_call32(addr);
#endif
}

void debug_uart_init(void)
{
}

static inline void _debug_uart_putc(int ch)
{
	struct efi_system_table *sys_table = global_sys_table;
	uint16_t ucode[2];

	ucode[0] = ch;
	ucode[1] = '\0';
	sys_table->con_out->output_string(sys_table->con_out, ucode);
}

DEBUG_UART_FUNCS

asmlinkage void printhex16(uint value) \
	{ \
		printhex(value, 16); \
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
	struct efi_simple_text_output_protocol *con_out = sys_table->con_out;
	struct efi_boot_services *boot = sys_table->boottime;
	int ret;

	global_sys_table = sys_table;
	con_out->output_string(con_out, L"U-Boot EFI Stub\n");

	struct efi_memory_descriptor desc[100];
	u32 version;
	ulong key, desc_size, size;

	size = sizeof(desc);
	ret = boot->get_memory_map(&size, desc, &key, &desc_size, &version);
	if (ret) {
		con_out->output_string(con_out, L"Can't get memory map\n");
		return ret;
	}
	printascii("hello\n");
	con_out->output_string(con_out, L"Got memmap\n");
	printascii("\nimage "); printhex8((ulong)image);
	printascii("\ndesc size "); printhex8(desc_size);
	printascii("\nkey "); printhex8(key >> 32); printhex8(key);
	printascii("\nsizeof(key) "); printhex8(sizeof(key));
	printascii("\nversion "); printhex8(version);
	printascii("\nsize "); printhex8(size);
	printascii("\ncount "); printhex8(size / desc_size);

	u64 count;
	boot->get_next_monotonic_count(&count);
	printascii("\nmono "); printhex8(count);
	boot->get_next_monotonic_count(&count);
	printascii("\nmono "); printhex8(count);

	ret = boot->exit_boot_services(image, key);
	if (ret) {
		con_out->output_string(con_out, L"Can't exit boot services\n");
		printhex8(ret);
		size = sizeof(desc);
		ret = boot->get_memory_map(&size, desc, &key, &desc_size, &version);
		if (ret) {
			con_out->output_string(con_out, L"Can't get memory map\n");
			printhex8(ret);
			return ret;
		}
		ret = boot->exit_boot_services(image, key);
		if (ret) {
			con_out->output_string(con_out, L"Can't exit boot services\n");
			printhex8(ret);
			return ret;
		}
	}

	copy_uboot(CONFIG_SYS_TEXT_BASE, (ulong)__u_boot_bin_begin,
		   ALIGN((ulong)__u_boot_bin_end, 8));
	jump_to_uboot(CONFIG_SYS_TEXT_BASE);

	return EFI_SUCCESS;
}
