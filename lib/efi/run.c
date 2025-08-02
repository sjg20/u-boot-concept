// SPDX-License-Identifier: GPL-2.0+
/**
 * Common functions for running EFI binaries
 *
 * Copyright 2025 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <blk.h>
#include <bootflow.h>
#include <dm.h>
#include <efi.h>
#include <mapmem.h>

/**
 * calculate_paths() - Calculate the device and image patch from strings
 *
 * @dev:		device, e.g. "MMC"
 * @devnr:		number of the device, e.g. "1:2"
 * @path:		path to file loaded
 * @device_pathp:	returns EFI device path
 * @image_pathp:	returns EFI image path
 * Return: EFI_SUCCESS on success, else error code
 */
static efi_status_t calculate_paths(const char *dev, const char *devnr,
				    const char *path,
				    struct efi_device_path **device_pathp,
				    struct efi_device_path **image_pathp)
{
	struct efi_device_path *image, *device;
	efi_status_t ret;

#if IS_ENABLED(CONFIG_NETDEVICES)
	if (!strcmp(dev, "Net") || !strcmp(dev, "Http")) {
		ret = efi_net_new_dp(dev, devnr, eth_get_dev());
		if (ret != EFI_SUCCESS)
			return ret;
	}
#endif

	ret = efi_dp_from_name(dev, devnr, path, &device, &image);
	if (ret != EFI_SUCCESS)
		return ret;

	*device_pathp = device;
	if (image) {
		/* FIXME: image should not contain device */
		struct efi_device_path *image_tmp = image;

		efi_dp_split_file_path(image, &device, &image);
		efi_free_pool(image_tmp);
	}
	*image_pathp = image;
	log_debug("- boot device %pD\n", device);
	if (image)
		log_debug("- image %pD\n", image);

	return EFI_SUCCESS;
}

/**
 * calc_dev_name() - Calculate the device name to give to EFI
 *
 * If not supported, this shows an error.
 *
 * Return name, or NULL if not supported
 */
static const char *calc_dev_name(struct bootflow *bflow)
{
	const struct udevice *media_dev;

	media_dev = dev_get_parent(bflow->dev);

	if (!bflow->blk) {
		if (device_get_uclass_id(media_dev) == UCLASS_ETH)
			return "Net";

		log_err("Cannot boot EFI app on media '%s'\n",
			dev_get_uclass_name(media_dev));

		return NULL;
	}

	if (device_get_uclass_id(media_dev) == UCLASS_MASS_STORAGE)
		return "usb";

	return blk_get_uclass_name(device_get_uclass_id(media_dev));
}

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

	dev_name = calc_dev_name(bflow);
	log_debug("dev_name '%s' devnum_str '%s' fname '%s' media_dev '%s'\n",
		  dev_name, devnum_str, bflow->fname, media_dev->name);
	if (!dev_name)
		return EFI_UNSUPPORTED;
	ret = calculate_paths(dev_name, devnum_str, bflow->fname, &device,
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
	if (IS_ENABLED(CONFIG_EFI_LOADER)) {
		ret = efi_binary_run_dp(bflow->buf, bflow->size, fdt, NULL, 0,
					device, image);
	} else {
		return EFI_UNSUPPORTED;
	}

	return ret;
}
