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
#include <linux/delay.h>

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
efi_status_t calculate_paths(const char *dev, const char *devnr,
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
	log_debug("dev '%s' devnr '%s' path '%s'\n", dev, devnr, path);
	ret = efi_dp_from_name(dev, devnr, path, &device, &image);
	if (ret != EFI_SUCCESS)
		return ret;

	if (image) {
		/* FIXME: image should not contain device */
		struct efi_device_path *image_tmp = image;

		efi_dp_split_file_path(image, &device, &image);
		efi_free_pool(image_tmp);
	}
	*device_pathp = device;
	*image_pathp = image;
	log_info("- boot device %pD\n", device);
	if (image)
		log_info("- image %pD\n", image);
	// mdelay(5000);

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
	log_debug("bflow->dev='%s', media_dev='%s', uclass_id=%d\n",
		  bflow->dev->name, media_dev->name,
		  device_get_uclass_id(media_dev));

	if (!bflow->blk) {
		if (device_get_uclass_id(media_dev) == UCLASS_ETH)
			return "Net";

		log_err("Cannot boot EFI app on media '%s'\n",
			dev_get_uclass_name(media_dev));

		return NULL;
	}

	if (device_get_uclass_id(media_dev) == UCLASS_MASS_STORAGE)
		return "usb";

	if (device_get_uclass_id(media_dev) == UCLASS_EFI_MEDIA)
		return "efi";

	return blk_get_uclass_name(device_get_uclass_id(media_dev));
}

/*
static efi_status_t EFIAPI my_handle_protocol(efi_handle_t handle,
					      const efi_guid_t *protocol,
					      void **protocol_interface)
{
	struct efi_priv *priv = efi_get_priv();
	efi_status_t ret;

	printf("call\n");
	ret = priv->orig_handle_protocol(handle, protocol, protocol_interface);
	printf("ret %lx\n\n", ret);

	return ret;
}
*/

#define USE_BUF		1

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

	//"/efi/boot/fakename.efi"
	//bflow->fname,
	ret = calculate_paths(dev_name, devnum_str,
			      USE_BUF ? "/efi/boot/fakename.efi" : bflow->fname,
			      &device, &image);
	if (ret)
		return EFI_UNSUPPORTED;

	if (bflow->flags & BOOTFLOWF_USE_BUILTIN_FDT) {
		log_debug("Booting with built-in fdt\n");
		fdt = EFI_FDT_USE_INTERNAL;
	} else {
		log_debug("Booting with external fdt\n");
		fdt = map_sysmem(bflow->fdt_addr, 0);
	}

	log_info("efi_bootflow_run(): device %pD\n", device);
#if USE_BUF /* create a loaded image from the buffer */
	ret = efi_run_image(bflow->buf, bflow->size, device, image);
	// ret = efi_binary_run_dp(bflow->buf, bflow->size, fdt, NULL, 0, device,
				// image);
#else
	struct efi_boot_services *boot = efi_get_boot();
	// struct efi_priv *priv = efi_get_priv();
	struct efi_device_path *file_path;
	efi_handle_t handle;

	file_path = efi_dp_concat(device, image, 0);

	log_info("loading image %pD\n", file_path);
	ret = boot->load_image(false, efi_get_parent_image(), file_path,
			       NULL, 0, &handle);
	if (ret) {
		log_err("Failed to load image\n");
		return ret;
	}
	// priv->orig_handle_protocol = boot->handle_protocol;
	// boot->handle_protocol = my_handle_protocol;
	ret = do_bootefi_exec(handle, NULL);
	// boot->handle_protocol = priv->orig_handle_protocol;
#endif
	printf("boot failed: ret=%lx\n", ret);

	return ret;
}
