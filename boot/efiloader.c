// SPDX-License-Identifier: GPL-2.0+
/*
 * EFI loader implementation for bootflow
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <blk.h>
#include <bootdevice.h>
#include <command.h>
#include <distro.h>
#include <dm.h>
#include <efi_loader.h>
#include <fs.h>
#include <malloc.h>
#include <mapmem.h>
#include <net.h>
#include <pxe_utils.h>
#include <vsprintf.h>

/* This could be written in C perhaps - taken from config_distro_bootcmd.h */
#if defined(CONFIG_ARM64)
#define BOOTEFI_NAME "bootaa64.efi"
#elif defined(CONFIG_ARM)
#define BOOTEFI_NAME "bootarm.efi"
#elif defined(CONFIG_X86_RUN_32BIT)
#define BOOTEFI_NAME "bootia32.efi"
#elif defined(CONFIG_X86_RUN_64BIT)
#define BOOTEFI_NAME "bootx64.efi"
#elif defined(CONFIG_ARCH_RV32I)
#define BOOTEFI_NAME "bootriscv32.efi"
#elif defined(CONFIG_ARCH_RV64I)
#define BOOTEFI_NAME "bootriscv64.efi"
#elif defined(CONFIG_SANDBOX)
#define BOOTEFI_NAME "bootsbox.efi"
#else
#error "Not supported for this architecture"
#endif

#define EFI_FNAME	"efi/boot/" BOOTEFI_NAME

int efiloader_boot_setup(struct blk_desc *desc, int partnum,
			 struct bootflow *bflow)
{
	loff_t size;
	int ret;

	bflow->type = BOOTFLOWT_EFILOADER;
	bflow->fname = strdup(EFI_FNAME);
	if (!bflow->fname)
		return log_msg_ret("name", -ENOMEM);
	ret = fs_size(bflow->fname, &size);
	bflow->size = size;
	if (ret)
		return log_msg_ret("size", ret);
	bflow->state = BOOTFLOWST_FILE;
	log_debug("   - distro file size %x\n", (uint)size);
	if (size > 0x2000000)
		return log_msg_ret("chk", -E2BIG);

	ret = efiload_read_file(desc, partnum, bflow);
	if (ret)
		return log_msg_ret("read", -EINVAL);

	return 0;
}

int efiloader_boot(struct bootflow *bflow)
{
	char cmd[50];

	/*
	 * At some point we can add a real interface to bootefi so we can call
	 * this directly. For now, go through the CLI like distro boot.
	 */
	snprintf(cmd, sizeof(cmd), "bootefi %lx %lx",
		 (ulong)map_to_sysmem(bflow->buf),
		 (ulong)map_to_sysmem(gd->fdt_blob));
	if (run_command(cmd, 0))
		return log_msg_ret("run", -EINVAL);

	return 0;
}
