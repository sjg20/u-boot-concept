/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * When a boot option does not provide a file path the EFI file to be
 * booted is \EFI\BOOT\$(BOOTEFI_NAME).EFI. The architecture specific
 * file name is defined in this include.
 *
 * Copyright (c) 2022, Heinrich Schuchardt <xypron.glpk@gmx.de>
 * Copyright (c) 2022, Linaro Limited
 */

#include <efi.h>
#include <errno.h>
#include <host_arch.h>

#if HOST_ARCH == HOST_ARCH_X86_64
#define HOST_BOOTEFI_NAME "BOOTX64.EFI"
#define HOST_PXE_ARCH 0x6
#elif HOST_ARCH == HOST_ARCH_X86
#define HOST_BOOTEFI_NAME "BOOTIA32.EFI"
#define HOST_PXE_ARCH 0x7
#elif HOST_ARCH == HOST_ARCH_AARCH64
#define HOST_BOOTEFI_NAME "BOOTAA64.EFI"
#define HOST_PXE_ARCH 0xb
#elif HOST_ARCH == HOST_ARCH_ARM
#define HOST_BOOTEFI_NAME "BOOTARM.EFI"
#define HOST_PXE_ARCH 0xa
#elif HOST_ARCH == HOST_ARCH_RISCV32
#define HOST_BOOTEFI_NAME "BOOTRISCV32.EFI"
#define HOST_PXE_ARCH 0x19
#elif HOST_ARCH == HOST_ARCH_RISCV64
#define HOST_BOOTEFI_NAME "BOOTRISCV64.EFI"
#define HOST_PXE_ARCH 0x1b
#else
#error Unsupported Host architecture
#endif

#if defined(CONFIG_SANDBOX)
#define BOOTEFI_NAME "BOOTSBOX.EFI"
#elif defined(CONFIG_ARM64)
#define BOOTEFI_NAME "BOOTAA64.EFI"
#elif defined(CONFIG_ARM)
#define BOOTEFI_NAME "BOOTARM.EFI"
#elif defined(CONFIG_X86_64)
#define BOOTEFI_NAME "BOOTX64.EFI"
#elif defined(CONFIG_X86)
#define BOOTEFI_NAME "BOOTIA32.EFI"
#elif defined(CONFIG_ARCH_RV32I)
#define BOOTEFI_NAME "BOOTRISCV32.EFI"
#elif defined(CONFIG_ARCH_RV64I)
#define BOOTEFI_NAME "BOOTRISCV64.EFI"
#else
#error Unsupported UEFI architecture
#endif

const char *efi_get_basename(void)
{
	return efi_use_host_arch() ? HOST_BOOTEFI_NAME : BOOTEFI_NAME;
}

int efi_get_pxe_arch(void)
{
	if (efi_use_host_arch())
		return HOST_PXE_ARCH;

	/* http://www.iana.org/assignments/dhcpv6-parameters/dhcpv6-parameters.xml */
	if (IS_ENABLED(CONFIG_ARM64))
		return 0xb;
	else if (IS_ENABLED(CONFIG_ARM))
		return 0xa;
	else if (IS_ENABLED(CONFIG_X86_64))
		return 0x6;
	else if (IS_ENABLED(CONFIG_X86))
		return 0x7;
	else if (IS_ENABLED(CONFIG_ARCH_RV32I))
		return 0x19;
	else if (IS_ENABLED(CONFIG_ARCH_RV64I))
		return 0x1b;
	else if (IS_ENABLED(CONFIG_SANDBOX))
		return 0;	/* not used */

	return -EINVAL;
}
