// SPDX-License-Identifier: GPL-2.0+
/*
 *  For the code moved from cmd/bootefi.c
 *  Copyright (c) 2016 Alexander Graf
 */

#define LOG_CATEGORY LOGC_EFI

#include <blk.h>
#include <bootflow.h>
#include <dm.h>
#include <efi.h>
#include <efi_device_path.h>
#include <malloc.h>
#include <mapmem.h>

efi_status_t calculate_paths(const char *dev, const char *devnr, const char *path,
			     struct efi_device_path **device_pathp,
			     struct efi_device_path **image_pathp)

{
	struct efi_device_path *image, *device;
	efi_status_t ret;

	ret = efi_dp_from_name(dev, devnr, path, &device, &image);
	if (ret != EFI_SUCCESS)
		return ret;

	*device_pathp = device;
	if (image) {
		/* FIXME: image should not contain device */
		struct efi_device_path *image_tmp = image;

		efi_dp_split_file_path(image, &device, &image);
		free(image_tmp);
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

/**
 * do_bootefi_exec() - execute EFI binary
 *
 * The image indicated by @handle is started. When it returns the allocated
 * memory for the @load_options is freed.
 *
 * @handle:		handle of loaded image
 * @load_options:	load options
 * Return:		status code
 *
 * Load the EFI binary into a newly assigned memory unwinding the relocation
 * information, install the loaded image protocol, and call the binary.
 */
efi_status_t do_bootefi_exec(efi_handle_t handle, void *load_options)
{
	efi_status_t ret;
	efi_uintn_t exit_data_size = 0;
	u16 *exit_data = NULL;
	struct efi_event *evt;

	/* On ARM switch from EL3 or secure mode to EL2 or non-secure mode */
	switch_to_non_secure_mode();

	/*
	 * The UEFI standard requires that the watchdog timer is set to five
	 * minutes when invoking an EFI boot option.
	 *
	 * Unified Extensible Firmware Interface (UEFI), version 2.7 Errata A
	 * 7.5. Miscellaneous Boot Services - EFI_BOOT_SERVICES.SetWatchdogTimer
	 */
	ret = efi_set_watchdog(300);
	if (ret != EFI_SUCCESS) {
		log_err("failed to set watchdog timer\n");
		goto out;
	}

	/* Call our payload! */
	ret = EFI_CALL(efi_start_image(handle, &exit_data_size, &exit_data));
	if (ret != EFI_SUCCESS) {
		log_err("## Application failed, r = %lu\n",
			ret & ~EFI_ERROR_MASK);
		if (exit_data) {
			log_err("## %ls\n", exit_data);
			efi_free_pool(exit_data);
		}
	}

out:
	free(load_options);

	/* Notify EFI_EVENT_GROUP_RETURN_TO_EFIBOOTMGR event group. */
	list_for_each_entry(evt, &efi_events, link) {
		if (evt->group &&
		    !guidcmp(evt->group,
			     &efi_guid_event_group_return_to_efibootmgr)) {
			efi_signal_event(evt);
			EFI_CALL(systab.boottime->close_event(evt));
			break;
		}
	}

	/* Control is returned to U-Boot, disable EFI watchdog */
	efi_set_watchdog(0);

	return ret;
}

/**
 * efi_run_image() - run loaded UEFI image
 *
 * @source_buffer:	memory address of the UEFI image
 * @source_size:	size of the UEFI image
 * @device:		EFI device-path
 * @image:		EFI image-path
 * Return:		status code
 */
static efi_status_t efi_run_image(void *source_buffer, efi_uintn_t source_size,
				  struct efi_device_path *device,
				  struct efi_device_path *image)
{
	efi_handle_t handle;
	struct efi_device_path *msg_path, *file_path;
	efi_status_t ret;
	u16 *load_options;

	file_path = efi_dp_concat(device, image, 0);
	msg_path = image;

	log_info("Booting %pD\n", msg_path);

	ret = EFI_CALL(efi_load_image(false, efi_root, file_path, source_buffer,
				      source_size, &handle));
	if (ret != EFI_SUCCESS) {
		log_err("Loading image failed\n");
		goto out;
	}

	/* Transfer environment variable as load options */
	ret = efi_env_set_load_options(handle, "bootargs", &load_options);
	if (ret != EFI_SUCCESS)
		goto out;

	ret = do_bootefi_exec(handle, load_options);

out:

	return ret;
}

efi_status_t efi_binary_run_dp(void *image_ptr, size_t size, void *fdt,
			       struct efi_device_path *device,
			       struct efi_device_path *image)
{
	efi_status_t ret;

	/* Initialize EFI drivers */
	ret = efi_init_obj_list();
	if (ret != EFI_SUCCESS) {
		log_err("Error: Cannot initialize UEFI sub-system, r = %lu\n",
			ret & ~EFI_ERROR_MASK);
		return -1;
	}

	ret = efi_install_fdt(fdt);
	if (ret != EFI_SUCCESS)
		return ret;

	return efi_run_image(image_ptr, size, device, image);
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
	ret = efi_binary_run_dp(bflow->buf, bflow->size, fdt, device, image);

	return ret;
}
