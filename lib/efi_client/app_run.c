// SPDX-License-Identifier: GPL-2.0+
/*
 * Running programs from the EFI app
 *
 * Copyright 2024 Linaro Limited
 * Copyright 2024 Canonical Ltd
 */

#define LOG_CATEGORY	LOGC_EFI

#include <bootm.h>
#include <dm.h>
#include <efi.h>
#include <efi_api.h>
#include <efi_device_path.h>
#include <log.h>
#include <malloc.h>
#include <linux/delay.h>

/**
 * do_bootefi_exec() - execute EFI binary
 *
 * The image indicated by @handle is started. When it returns the allocated
 * memory for the @load_options is freed.
 *
 * @handle:		handle of loaded image
 * @load_options:	load options
 * Return:		status code
 */
efi_status_t do_bootefi_exec(efi_handle_t handle, void *load_options)
{
	struct efi_boot_services *boot = efi_get_boot();
	efi_uintn_t exit_data_size = 0;
	u16 *exit_data = NULL;
	efi_status_t ret;
	struct efi_loaded_image *image;
	struct efi_device_path *dp;

	/* On ARM switch from EL3 or secure mode to EL2 or non-secure mode */
	if (!IS_ENABLED(CONFIG_EFI_APP))
		switch_to_non_secure_mode();

	/* TODO(sjg@chromium.org): Set watchdog */

	log_info("Looking at loading image\n");
	ret = boot->handle_protocol(handle, &efi_guid_loaded_image,
				   (void **)&image);
	if (ret) {
		log_err("Failed to get loaded image\n");
		return ret;
	}
	printf("done image=%p\n", image);

	printf("loaded image path %ls\n", efi_dp_str(image->file_path));
	printf("device_handle %p\n", image->device_handle);

	ret = boot->handle_protocol(handle, &efi_guid_loaded_image_device_path,
				   (void **)&dp);
	printf("device path %ls\n", efi_dp_str(dp));

	mdelay(5000);

	/* Call our payload! */
	ret = boot->start_image(handle, &exit_data_size, &exit_data);
	if (ret != EFI_SUCCESS) {
		log_err("## Application failed, r = %lu\n",
			ret & ~EFI_ERROR_MASK);
		if (exit_data) {
			log_err("## %ls\n", exit_data);
			boot->free_pool(exit_data);
		}
	}

	free(load_options);

	/* TODO(sjg@chromium.org): Disable watchdog */

	return ret;
}

efi_status_t efi_run_image(void *source_buffer, efi_uintn_t source_size,
			   struct efi_device_path *dp_dev,
			   struct efi_device_path *dp_img)
{
	struct efi_boot_services *boot = efi_get_boot();
	efi_handle_t handle, device_handle = NULL;
	struct efi_device_path *msg_path, *file_path;
	efi_status_t ret;

	log_info("efi_run_image():\n");
	log_info("dp_dev %pD\n", dp_dev);
	log_info("dp_img %pD\n", dp_img);
	file_path = efi_dp_concat(dp_dev, dp_img, 0);
	msg_path = dp_img;

	log_info("Booting %pD\n", msg_path);
	log_info("file_path %pD\n", file_path);

	/* Create a device handle and install device path protocol */
	ret = boot->install_protocol_interface(&device_handle,
					       &efi_guid_device_path,
					       EFI_NATIVE_INTERFACE, dp_dev);
	if (ret != EFI_SUCCESS) {
		log_err("Failed to install device path protocol\n");
		goto out;
	}

	ret = boot->load_image(false, efi_get_parent_image(), file_path,
			       source_buffer, source_size, &handle);
	if (ret != EFI_SUCCESS) {
		log_err("Loading image failed\n");
		goto cleanup;
	}

	ret = do_bootefi_exec(handle, NULL);

cleanup:
	printf("cleanup\n");
	if (device_handle) {
		boot->uninstall_protocol_interface(device_handle,
						   &efi_guid_device_path,
						   dp_dev);
	}
out:
	printf("file-path\n");
	if (file_path)
		efi_free_pool(file_path);
	printf("returning\n");

	return ret;
}

efi_status_t efi_binary_run_dp(void *image, size_t size, void *fdt,
			       void *initrd, size_t initrd_sz,
			       struct efi_device_path *dp_dev,
			       struct efi_device_path *dp_img)
{
	log_info("efi_bootflow_run(): dp_dev %pD\n", dp_dev);
	return efi_run_image(image, size, dp_dev, dp_img);
}

int efi_dp_from_bootdev(const struct udevice *dev,
			const struct efi_device_path **dpp)
{
	const struct udevice *media = dev_get_parent(dev);
	const struct efi_media_plat *plat;

	log_debug("dev '%s': uclass ID %d\n", media->name,
		  device_get_uclass_id(media));
	if (device_get_uclass_id(media) != UCLASS_EFI_MEDIA)
		return log_msg_ret("efb", -ENOTSUPP);

	plat = dev_get_plat(media);
	*dpp = plat->device_path;

	return 0;
}
