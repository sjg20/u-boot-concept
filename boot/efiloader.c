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

static int efiload_read_file(struct blk_desc *desc, int partnum,
			     struct bootflow *bflow)
{
	const struct udevice *media_dev;
	int size = bflow->size;
	char devnum_str[9];
	char dirname[200];
	loff_t bytes_read;
	char *last_slash;
	ulong addr;
	char *buf;
	int ret;

	/* Sadly FS closes the file after fs_size() so we must redo this */
	ret = fs_set_blk_dev_with_part(desc, partnum);
	if (ret)
		return log_msg_ret("set", ret);

	buf = malloc(size + 1);
	if (!buf)
		return log_msg_ret("buf", -ENOMEM);
	addr = map_to_sysmem(buf);

	ret = fs_read(bflow->fname, addr, 0, 0, &bytes_read);
	if (ret) {
		free(buf);
		return log_msg_ret("read", ret);
	}
	if (size != bytes_read)
		return log_msg_ret("bread", -EINVAL);
	buf[size] = '\0';
	bflow->state = BOOTFLOWST_LOADED;
	bflow->buf = buf;

	/*
	 * This is a horrible hack to tell EFI about this boot device. Once we
	 * unify EFI with the rest of U-Boot we can clean this up. The same hack
	 * exists in multiple places, e.g. in the fs, tftp and load commands.
	 *
	 * Once we can clean up the EFI code to make proper use of driver model,
	 * this can go away.
	 */
	media_dev = dev_get_parent(bflow->dev);
	snprintf(devnum_str, sizeof(devnum_str), "%x", dev_seq(media_dev));

	strlcpy(dirname, bflow->fname, sizeof(dirname));
	last_slash = strrchr(dirname, '/');
	if (last_slash)
		*last_slash = '\0';

	efi_set_bootdev(dev_get_uclass_name(media_dev), devnum_str, dirname,
			bflow->buf, size);

	return 0;
}

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
