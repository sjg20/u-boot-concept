// SPDX-License-Identifier: GPL-2.0+
/*
 * Aurora Innovation, Inc. Copyright 2022.
 *
 */

#include <blk.h>
#include <command.h>
#include <dm.h>
#include <env.h>
#include <part.h>
#include <efi.h>
#include <efi_api.h>

static bool partition_is_on_device(const struct efi_device_path *device,
				   const struct efi_device_path *part,
				   __u32 *part_no)
{
	size_t d_len, p_len;
	const struct efi_device_path *p, *d;

	for (d = device; d->type != DEVICE_PATH_TYPE_END; d = (void *)d + d->length) {
	}

	d_len = (void *)d - (void *)device;

	for (p = part; p->type != DEVICE_PATH_TYPE_END &&
		     !(p->type == DEVICE_PATH_TYPE_MEDIA_DEVICE &&
		       p->sub_type == DEVICE_PATH_SUB_TYPE_HARD_DRIVE_PATH);
	     p = (void *)p + p->length) {
	}

	if (p->type != DEVICE_PATH_TYPE_MEDIA_DEVICE ||
	    p->sub_type != DEVICE_PATH_SUB_TYPE_HARD_DRIVE_PATH) {
		// Not a partition.
		return false;
	}

	p_len = (void *)p - (void *)part;

	if (p_len == d_len && !memcmp(device, part, p_len)) {
		if (part_no)
			*part_no = ((__u32 *)p)[1];
		return true;
	}
	return false;
}

/**
 * part_self_find() - Check if a device contains the loaded-image path
 *
 * @udev: Block device to check
 * @loaded_image_path: EFI path of the loaded image
 * Return 0 if found, -ENOENT if not, other -ve value on error
 */
static int part_self_find(struct udevice *udev,
			  struct efi_device_path *loaded_image_path)
{
	struct blk_desc *desc = dev_get_uclass_plat(udev);

	if (desc->uclass_id == UCLASS_EFI_MEDIA) {
		struct efi_media_plat *plat = dev_get_plat(udev->parent);
		u32 loader_part_no;

		if (partition_is_on_device(plat->device_path, loaded_image_path,
					   &loader_part_no)) {
			char env[256];
			int ret;

			ret = snprintf(env, sizeof(env), "%s %x:%x",
				       blk_get_uclass_name(desc->uclass_id),
				       desc->devnum, loader_part_no);
			if (ret < 0 || ret == sizeof(env))
				return -ENOSPC;
			if (env_set("target_part", env))
				return -ENOMEM;
			return 0;
		}
	}

	return -ENOENT;
}

/**
 * part_blk_find() - Check if a device contains a partition with a type uuid
 *
 * @udev: Block device to check
 * @uuid: UUID to search for (in string form)
 * Return 0 if found, -ENOENT if not, other -ve value on error
 */
static int part_blk_find(struct udevice *udev, const char *uuid)
{
	struct blk_desc *desc = dev_get_uclass_plat(udev);
	int i;

	for (i = 1; i <= MAX_SEARCH_PARTITIONS; i++) {
		struct disk_partition info;
		int ret;

		ret = part_get_info(desc, i, &info);
		if (ret)
			break;
		if (strcasecmp(uuid, info.type_guid) == 0) {
			char env[256];

			ret = snprintf(env, sizeof(env), "%s %x:%x",
				       blk_get_uclass_name(desc->uclass_id),
				       desc->devnum, i);
			if (ret < 0 || ret == sizeof(env))
				return -ENOSPC;
			debug("Setting target_part to %s\n", env);
			if (env_set("target_part", env))
				return -ENOMEM;
			return 0;
		}
	}

	return -ENOENT;
}

static int part_find(int argc, char *const argv[])
{
	efi_guid_t efi_devpath_guid = EFI_DEVICE_PATH_PROTOCOL_GUID;
	struct efi_device_path *loaded_image_path = NULL;
	bool part_self = false;
	struct driver *d = ll_entry_start(struct driver, driver);
	const int n_ents = ll_entry_count(struct driver, driver);
	struct driver *entry;
	struct udevice *udev;
	struct uclass *uc;
	int ret;

	if (argc != 2)
		return CMD_RET_USAGE;

	if (IS_ENABLED(CONFIG_EFI)) {
		struct efi_boot_services *boot = efi_get_boot();
		struct efi_priv *priv = efi_get_priv();

		part_self = !strncmp(argv[1], "self", 6);
		if (part_self) {
			ret = boot->handle_protocol(priv->loaded_image->device_handle,
						    &efi_devpath_guid,
						    (void **)&loaded_image_path);
			if (ret)
				log_warning("failed to get device path for loaded image (ret=%d)",
					    ret);
		}
	}

	ret = uclass_get(UCLASS_BLK, &uc);
	if (ret) {
		puts("Could not get BLK uclass.\n");
		return CMD_RET_FAILURE;
	}
	for (entry = d; entry < d + n_ents; entry++) {
		if (entry->id != UCLASS_BLK)
			continue;
		uclass_foreach_dev(udev, uc) {
			if (udev->driver != entry)
				continue;
			if (IS_ENABLED(CONFIG_EFI) && part_self)
				ret = part_self_find(udev, loaded_image_path);
			else
				ret = part_blk_find(udev, argv[1]);
			if (!ret)
				return 0;
			if (ret != -ENOENT)
				break;
		}
	}

	return CMD_RET_FAILURE;
}

static int do_part_find(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	return part_find(argc, argv);
}

U_BOOT_CMD(
	part_find, 2, 0, do_part_find, "Find a partition",
	"<guid>\n"
	"- Examine the list of known partitions for one that has a type\n"
	"  GUID that matches 'guid', expressed in the standard text format.\n"
	"  If successful, the target_part environment variable will be set\n"
	"  to the corresponding 'interface dev:part'.\n"
);
