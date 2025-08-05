// SPDX-License-Identifier: GPL-2.0+
/*
 * Running programs from the EFI app
 *
 * Copyright 2024 Linaro Limited
 * Copyright 2024 Canonical Ltd
 */

#include <bootm.h>
#include <efi.h>
#include <efi_api.h>
#include <efi_device_path.h>
#include <log.h>
#include <malloc.h>

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
static efi_status_t do_bootefi_exec(efi_handle_t handle, void *load_options)
{
	struct efi_boot_services *boot = efi_get_boot();
	efi_uintn_t exit_data_size = 0;
	u16 *exit_data = NULL;
	efi_status_t ret;

	/* On ARM switch from EL3 or secure mode to EL2 or non-secure mode */
	switch_to_non_secure_mode();

	/* TODO(sjg@chromium.org): Set watchdog */

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
	efi_handle_t handle;
	struct efi_device_path *msg_path, *file_path;
	efi_status_t ret;

	file_path = efi_dp_concat(dp_dev, dp_img, 0);
	msg_path = dp_img;

	log_info("Booting %pD\n", msg_path);

	ret = boot->load_image(false, efi_get_parent_image(), file_path,
			       source_buffer, source_size, &handle);
	if (ret != EFI_SUCCESS) {
		log_err("Loading image failed\n");
		goto out;
	}

	ret = do_bootefi_exec(handle, NULL);

out:

	return ret;
}

efi_status_t efi_binary_run_dp(void *image, size_t size, void *fdt,
			       void *initrd, size_t initrd_sz,
			       struct efi_device_path *dp_dev,
			       struct efi_device_path *dp_img)
{
	return efi_run_image(image, size, dp_dev, dp_img);
}
