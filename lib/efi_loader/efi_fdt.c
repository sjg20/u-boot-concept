// SPDX-License-Identifier: GPL-2.0+
/*
 * Bootmethod for distro boot via EFI
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY LOGC_EFI

#include <efi_device_path.h>
#include <efi_loader.h>
#include <env.h>
#include <log.h>
#include <string.h>
#include <vsprintf.h>

/**
 * efi_load_distro_fdt() - load distro device-tree
 *
 * @handle:	handle of loaded image
 * @fdt:	on return device-tree, must be freed via efi_free_pages()
 * @fdt_size:	buffer size
 */
void efi_load_distro_fdt(efi_handle_t handle, void **fdt, efi_uintn_t *fdt_size)
{
	struct efi_device_path *dp;
	efi_status_t  ret;
	struct efi_handler *handler;
	struct efi_loaded_image *loaded_image;
	efi_handle_t device;

	*fdt = NULL;

	/* Get boot device from loaded image protocol */
	ret = efi_search_protocol(handle, &efi_guid_loaded_image, &handler);
	if (ret != EFI_SUCCESS)
		return;
	loaded_image = handler->protocol_interface;
	device = loaded_image->device_handle;

	/* Get device path of boot device */
	ret = efi_search_protocol(device, &efi_guid_device_path, &handler);
	if (ret != EFI_SUCCESS)
		return;
	dp = handler->protocol_interface;

	/* Try the various available names */
	for (int seq = 0; ; ++seq) {
		struct efi_device_path *file;
		char buf[255];

		if (efi_get_distro_fdt_name(buf, sizeof(buf), seq))
			break;
		file = efi_dp_from_file(dp, buf);
		if (!file)
			break;
		ret = efi_load_image_from_path(true, file, fdt, fdt_size);
		efi_free_pool(file);
		if (ret == EFI_SUCCESS) {
			log_debug("Fdt %pD loaded\n", file);
			break;
		}
	}
}
