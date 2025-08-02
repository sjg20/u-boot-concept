// SPDX-License-Identifier: GPL-2.0+
/*
 * EFI device path from u-boot device-model mapping
 *
 * (C) Copyright 2017 Rob Clark
 */

#define LOG_CATEGORY LOGC_EFI

#include <blk.h>
#include <bootflow.h>
#include <dm.h>
#include <efi.h>
#include <efi_device_path.h>
#include <log.h>

efi_status_t efi_calculate_paths(const char *dev, const char *devnr,
			         const char *path,
				 struct efi_device_path **device_pathp,
				 struct efi_device_path **image_pathp)
{
	struct efi_device_path *image, *device;
	efi_status_t ret;

#if IS_ENABLED(CONFIG_NETDEVICES) && IS_ENABLED(CONFIG_EFI_LOADER)
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

const char *efi_calc_dev_name(struct bootflow *bflow)
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
