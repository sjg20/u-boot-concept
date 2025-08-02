// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2016 Alexander Graf
 * Copyright 2023 Google LLC
 */

#define LOG_CATEGORY LOGC_EFI

#include <blk.h>
#include <bootflow.h>
#include <dm.h>
#include <efi.h>
#include <efi_device_path.h>
#include <efi_loader.h>
#include <malloc.h>
#include <mapmem.h>
#include <net-common.h>

efi_status_t efi_bootflow_run(struct bootflow *bflow)
{
	struct efi_device_path *device, *image;
	const struct udevice *media_dev;
	struct blk_desc *desc = NULL;
	const char *dev_name;
	char devnum_str[9];
	efi_status_t ret;
	void *fdt;

	media_dev = dev_get_parent(bflow->dev);
	if (bflow->blk) {
		desc = dev_get_uclass_plat(bflow->blk);

		snprintf(devnum_str, sizeof(devnum_str), "%x:%x",
			 desc ? desc->devnum : dev_seq(media_dev), bflow->part);
	} else {
		*devnum_str = '\0';
	}

	dev_name = efi_calc_dev_name(bflow);
	log_debug("dev_name '%s' devnum_str '%s' fname '%s' media_dev '%s'\n",
		  dev_name, devnum_str, bflow->fname, media_dev->name);
	if (!dev_name)
		return EFI_UNSUPPORTED;
	ret = efi_calculate_paths(dev_name, devnum_str, bflow->fname, &device,
				  &image);
	if (ret)
		return EFI_UNSUPPORTED;

	if (bflow->flags & BOOTFLOWF_USE_BUILTIN_FDT) {
		log_debug("Booting with built-in fdt\n");
		fdt = EFI_FDT_USE_INTERNAL;
	} else {
		log_debug("Booting with external fdt\n");
		fdt = map_sysmem(bflow->fdt_addr, 0);
	}

	ret = efi_binary_run_dp(bflow->buf, bflow->size, fdt, NULL, 0, device,
				image);

	return ret;
}
