// SPDX-License-Identifier: GPL-2.0+
/*
 *  EFI setup code
 *
 *  Copyright (c) 2016 Alexander Graf
 *  Copyright (c) 2018 AKASHI Takahiro, Linaro Limited
 */

#include <common.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/root.h>
#include <efi_loader.h>

DECLARE_GLOBAL_DATA_PTR;

#define OBJ_LIST_NOT_INITIALIZED 1

static efi_status_t efi_obj_list_initialized = OBJ_LIST_NOT_INITIALIZED;

/* Initialize and populate EFI object list */
static efi_status_t efi_system_init(void)
{
	efi_status_t ret = EFI_SUCCESS;

	/*
	 * On the ARM architecture gd is mapped to a fixed register (r9 or x18).
	 * As this register may be overwritten by an EFI payload we save it here
	 * and restore it on every callback entered.
	 */
	efi_save_gd();

	/* Initialize once only */
	if (efi_obj_list_initialized != OBJ_LIST_NOT_INITIALIZED)
		return efi_obj_list_initialized;

	/* Initialize system table */
	ret = efi_initialize_system_table();
	if (ret != EFI_SUCCESS)
		goto out;

	/* Initialize root node */
	ret = efi_root_node_register();
	if (ret != EFI_SUCCESS)
		goto out;

	/* Initialize EFI driver uclass */
	ret = efi_driver_init();
	if (ret != EFI_SUCCESS)
		goto out;

#ifdef CONFIG_PARTITIONS
	ret = efi_disk_register();
	if (ret != EFI_SUCCESS)
		goto out;
#endif
#if defined(CONFIG_LCD)
	/* for !DM_VIDEO */
	ret = efi_gop_register();
	if (ret != EFI_SUCCESS)
		goto out;
#endif
#ifdef CONFIG_GENERATE_ACPI_TABLE
	ret = efi_acpi_register();
	if (ret != EFI_SUCCESS)
		goto out;
#endif
#ifdef CONFIG_GENERATE_SMBIOS_TABLE
	ret = efi_smbios_register();
	if (ret != EFI_SUCCESS)
		goto out;
#endif
	ret = efi_watchdog_register();
	if (ret != EFI_SUCCESS)
		goto out;

	/* Initialize EFI runtime services */
	ret = efi_reset_system_init();
	if (ret != EFI_SUCCESS)
		goto out;

out:
	efi_obj_list_initialized = ret;
	return ret;
}

/* For backward compatibility */
efi_status_t efi_init_obj_list(void)
{
	int ret;
	extern struct udevice *efi_root;

	if (efi_root)
		return EFI_SUCCESS;

	ret = device_bind_driver(dm_root(), "efi_root", "UEFI sub system",
				 &efi_root);
	if (ret)
		return EFI_OUT_OF_RESOURCES;

	ret = device_probe(efi_root);
	if (ret)
		return EFI_OUT_OF_RESOURCES;

	return EFI_SUCCESS;
}

static int efi_system_probe(struct udevice *dev)
{
	efi_status_t ret;

	ret = efi_system_init();
	if (ret != EFI_SUCCESS)
		return -1;

	return 0;
}

U_BOOT_DRIVER(efi_root) = {
	.name = "efi_root",
	.id = UCLASS_EFI,
	.probe = efi_system_probe,
};

UCLASS_DRIVER(efi) = {
	.name = "efi",
	.id = UCLASS_EFI,
};
