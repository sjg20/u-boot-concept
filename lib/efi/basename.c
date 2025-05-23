// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2022, Heinrich Schuchardt <xypron.glpk@gmx.de>
 */

#define LOG_CATEGORY LOGC_EFI

#include <efi.h>
#include <efi_load_initrd.h>
#include <env.h>
#include <errno.h>

#undef BOOTEFI_NAME

/*
 * The constants below come from:
 *
 * https://www.iana.org/assignments/dhcpv6-parameters/dhcpv6-parameters.xhtml#processor-architecture
 */

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

#if defined(CONFIG_CMD_EFIDEBUG) || defined(CONFIG_EFI_LOAD_FILE2_INITRD)
/* GUID used by Linux to identify the LoadFile2 protocol with the initrd */
const efi_guid_t efi_lf2_initrd_guid = EFI_INITRD_MEDIA_GUID;
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

	return -EINVAL;
}
