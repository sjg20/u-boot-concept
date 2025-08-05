// SPDX-License-Identifier: GPL-2.0+
/*
 * Root node for system services
 *
 * Copyright (c) 2018 Heinrich Schuchardt
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY LOGC_EFI

#include <efi.h>
#include <efi_api.h>
#include <malloc.h>

efi_handle_t efi_root = NULL;

struct efi_root_dp {
	struct efi_device_path_vendor vendor;
	struct efi_device_path end;
} __packed;

/**
 * efi_root_node_register() - create root node
 *
 * Create the root node on which we install all protocols that are
 * not related to a loaded image or a driver.
 *
 * Return:	status code
 */
efi_status_t efi_root_node_register(void)
{
	struct efi_boot_services *boot = efi_get_boot();
	efi_status_t ret;
	struct efi_root_dp *dp;

	/* Create device path protocol */
	dp = calloc(1, sizeof(*dp));
	if (!dp)
		return EFI_OUT_OF_RESOURCES;

	/* Fill vendor node */
	dp->vendor.dp.type = DEVICE_PATH_TYPE_HARDWARE_DEVICE;
	dp->vendor.dp.sub_type = DEVICE_PATH_SUB_TYPE_VENDOR;
	dp->vendor.dp.length = sizeof(struct efi_device_path_vendor);
	dp->vendor.guid = efi_u_boot_guid;

	/* Fill end node */
	dp->end.type = DEVICE_PATH_TYPE_END;
	dp->end.sub_type = DEVICE_PATH_SUB_TYPE_END;
	dp->end.length = sizeof(struct efi_device_path);

	/* Create root node and install protocols */
	ret = boot->install_multiple_protocol_interfaces
		(&efi_root,
		 /* Device path protocol */
		 &efi_guid_device_path, dp,
		 NULL);

	efi_root->type = EFI_OBJECT_TYPE_U_BOOT_FIRMWARE;

	return ret;
}
