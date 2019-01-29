// SPDX-License-Identifier: GPL-2.0+
/*
 *  EFI application disk support
 *
 *  Copyright (c) 2016 Alexander Graf
 */

#include <common.h>
#include <blk.h>
#include <dm.h>
#include <efi_loader.h>
#include <part.h>
#include <malloc.h>

const efi_guid_t efi_block_io_guid = BLOCK_IO_GUID;

static efi_status_t EFIAPI efi_disk_reset(struct efi_block_io *this,
			char extended_verification)
{
	EFI_ENTRY("%p, %x", this, extended_verification);
	return EFI_EXIT(EFI_DEVICE_ERROR);
}

enum efi_disk_direction {
	EFI_DISK_READ,
	EFI_DISK_WRITE,
};

static efi_status_t efi_disk_rw_blocks(struct efi_block_io *this,
			u32 media_id, u64 lba, unsigned long buffer_size,
			void *buffer, enum efi_disk_direction direction)
{
	struct efi_disk_obj *diskobj;
	struct blk_desc *desc;
	int blksz;
	int blocks;
	unsigned long n;

	diskobj = container_of(this, struct efi_disk_obj, ops);
	desc = container_of(diskobj, struct blk_desc, efi_disk);
	blksz = desc->blksz;
	blocks = buffer_size / blksz;
	lba += diskobj->offset;

	debug("EFI: %s:%d blocks=%x lba=%llx blksz=%x dir=%d\n", __func__,
	      __LINE__, blocks, lba, blksz, direction);

	/* We only support full block access */
	if (buffer_size & (blksz - 1))
		return EFI_DEVICE_ERROR;

	if (direction == EFI_DISK_READ)
		n = blk_dread(desc, lba, blocks, buffer);
	else
		n = blk_dwrite(desc, lba, blocks, buffer);

	/* We don't do interrupts, so check for timers cooperatively */
	efi_timer_check();

	debug("EFI: %s:%d n=%lx blocks=%x\n", __func__, __LINE__, n, blocks);

	if (n != blocks)
		return EFI_DEVICE_ERROR;

	return EFI_SUCCESS;
}

static efi_status_t EFIAPI efi_disk_read_blocks(struct efi_block_io *this,
			u32 media_id, u64 lba, efi_uintn_t buffer_size,
			void *buffer)
{
	void *real_buffer = buffer;
	efi_status_t r;

#ifdef CONFIG_EFI_LOADER_BOUNCE_BUFFER
	if (buffer_size > EFI_LOADER_BOUNCE_BUFFER_SIZE) {
		r = efi_disk_read_blocks(this, media_id, lba,
			EFI_LOADER_BOUNCE_BUFFER_SIZE, buffer);
		if (r != EFI_SUCCESS)
			return r;
		return efi_disk_read_blocks(this, media_id, lba +
			EFI_LOADER_BOUNCE_BUFFER_SIZE / this->media->block_size,
			buffer_size - EFI_LOADER_BOUNCE_BUFFER_SIZE,
			buffer + EFI_LOADER_BOUNCE_BUFFER_SIZE);
	}

	real_buffer = efi_bounce_buffer;
#endif

	EFI_ENTRY("%p, %x, %llx, %zx, %p", this, media_id, lba,
		  buffer_size, buffer);

	r = efi_disk_rw_blocks(this, media_id, lba, buffer_size, real_buffer,
			       EFI_DISK_READ);

	/* Copy from bounce buffer to real buffer if necessary */
	if ((r == EFI_SUCCESS) && (real_buffer != buffer))
		memcpy(buffer, real_buffer, buffer_size);

	return EFI_EXIT(r);
}

static efi_status_t EFIAPI efi_disk_write_blocks(struct efi_block_io *this,
			u32 media_id, u64 lba, efi_uintn_t buffer_size,
			void *buffer)
{
	void *real_buffer = buffer;
	efi_status_t r;

#ifdef CONFIG_EFI_LOADER_BOUNCE_BUFFER
	if (buffer_size > EFI_LOADER_BOUNCE_BUFFER_SIZE) {
		r = efi_disk_write_blocks(this, media_id, lba,
			EFI_LOADER_BOUNCE_BUFFER_SIZE, buffer);
		if (r != EFI_SUCCESS)
			return r;
		return efi_disk_write_blocks(this, media_id, lba +
			EFI_LOADER_BOUNCE_BUFFER_SIZE / this->media->block_size,
			buffer_size - EFI_LOADER_BOUNCE_BUFFER_SIZE,
			buffer + EFI_LOADER_BOUNCE_BUFFER_SIZE);
	}

	real_buffer = efi_bounce_buffer;
#endif

	EFI_ENTRY("%p, %x, %llx, %zx, %p", this, media_id, lba,
		  buffer_size, buffer);

	/* Populate bounce buffer if necessary */
	if (real_buffer != buffer)
		memcpy(real_buffer, buffer, buffer_size);

	r = efi_disk_rw_blocks(this, media_id, lba, buffer_size, real_buffer,
			       EFI_DISK_WRITE);

	return EFI_EXIT(r);
}

static efi_status_t EFIAPI efi_disk_flush_blocks(struct efi_block_io *this)
{
	/* We always write synchronously */
	EFI_ENTRY("%p", this);
	return EFI_EXIT(EFI_SUCCESS);
}

static const struct efi_block_io block_io_disk_template = {
	.reset = &efi_disk_reset,
	.read_blocks = &efi_disk_read_blocks,
	.write_blocks = &efi_disk_write_blocks,
	.flush_blocks = &efi_disk_flush_blocks,
};

/*
 * Get the simple file system protocol for a file device path.
 *
 * The full path provided is split into device part and into a file
 * part. The device part is used to find the handle on which the
 * simple file system protocol is installed.
 *
 * @full_path	device path including device and file
 * @return	simple file system protocol
 */
struct efi_simple_file_system_protocol *
efi_fs_from_path(struct efi_device_path *full_path)
{
	struct udevice *handle, *protocol;
	struct efi_handler *handler;
	struct efi_device_path *device_path;
	struct efi_device_path *file_path;
	efi_status_t ret;

	/* Split the path into a device part and a file part */
	ret = efi_dp_split_file_path(full_path, &device_path, &file_path);
	if (ret != EFI_SUCCESS)
		return NULL;
	efi_free_pool(file_path);

	/* Get the EFI object for the partition */
	handle = efi_dp_find_obj(device_path, NULL);
	efi_free_pool(device_path);
	if (!handle)
		return NULL;

	/* Find the simple file system protocol */
	ret = efi_search_protocol(handle, &efi_simple_file_system_protocol_guid,
				  &protocol);
	if (ret != EFI_SUCCESS)
		return NULL;

	/* Return the simple file system protocol for the partition */
	handler = protocol->uclass_platdata;
	return handler->protocol_interface;
}

#ifndef CONFIG_BLK
/*
 * Create a handle for a partition or disk
 *
 * @parent	parent handle
 * @dp_parent	parent device path
 * @if_typename interface name for block device
 * @desc	internal block device
 * @dev_index   device index for block device
 * @offset	offset into disk for simple partitions
 * @return	disk object
 */
static efi_status_t efi_disk_add_dev(
				efi_handle_t parent,
				struct efi_device_path *dp_parent,
				const char *if_typename,
				struct blk_desc *desc,
				int dev_index,
				lbaint_t offset,
				unsigned int part,
				struct efi_disk_obj **disk)
{
	struct efi_disk_obj *diskobj;
	efi_status_t ret;

	/* Don't add empty devices */
	if (!desc->lba)
		return EFI_NOT_READY;

	diskobj = calloc(1, sizeof(*diskobj));
	if (!diskobj)
		return EFI_OUT_OF_RESOURCES;

	/* Hook up to the device list */
	efi_add_handle(&diskobj->header);

	/* Fill in object data */
	if (part) {
		struct efi_device_path *node = efi_dp_part_node(desc, part);

		diskobj->dp = efi_dp_append_node(dp_parent, node);
		efi_free_pool(node);
	} else {
		diskobj->dp = efi_dp_from_part(desc, part);
	}
	diskobj->part = part;
	ret = efi_add_protocol(&diskobj->header, &efi_block_io_guid,
			       &diskobj->ops);
	if (ret != EFI_SUCCESS)
		return ret;
	ret = efi_add_protocol(&diskobj->header, &efi_guid_device_path,
			       diskobj->dp);
	if (ret != EFI_SUCCESS)
		return ret;
	if (part >= 1) {
		diskobj->volume = efi_simple_file_system(desc, part,
							 diskobj->dp);
		ret = efi_add_protocol(&diskobj->header,
				       &efi_simple_file_system_protocol_guid,
				       diskobj->volume);
		if (ret != EFI_SUCCESS)
			return ret;
	}
	diskobj->ops = block_io_disk_template;
	diskobj->ifname = if_typename;
	diskobj->dev_index = dev_index;
	diskobj->offset = offset;
	diskobj->desc = desc;

	/* Fill in EFI IO Media info (for read/write callbacks) */
	diskobj->media.removable_media = desc->removable;
	diskobj->media.media_present = 1;
	diskobj->media.block_size = desc->blksz;
	diskobj->media.io_align = desc->blksz;
	diskobj->media.last_block = desc->lba - offset;
	if (part != 0)
		diskobj->media.logical_partition = 1;
	diskobj->ops.media = &diskobj->media;
	if (disk)
		*disk = diskobj;
	return EFI_SUCCESS;
}

/*
 * Create handles and protocols for the partitions of a block device
 *
 * @parent		handle of the parent disk
 * @blk_desc		block device
 * @if_typename		interface type
 * @diskid		device number
 * @pdevname		device name
 * @return		number of partitions created
 */
int efi_disk_create_partitions(efi_handle_t parent, struct blk_desc *desc,
			       const char *if_typename, int diskid,
			       const char *pdevname)
{
	int disks = 0;
	char devname[32] = { 0 }; /* dp->str is u16[32] long */
	disk_partition_t info;
	int part;
	struct efi_device_path *dp = NULL;
	efi_status_t ret;
	struct efi_handler *handler;

	/* Get the device path of the parent */
	ret = efi_search_protocol(parent, &efi_guid_device_path, &handler);
	if (ret == EFI_SUCCESS)
		dp = handler->protocol_interface;

	/* Add devices for each partition */
	for (part = 1; part <= MAX_SEARCH_PARTITIONS; part++) {
		if (part_get_info(desc, part, &info))
			continue;
		snprintf(devname, sizeof(devname), "%s:%d", pdevname,
			 part);
		ret = efi_disk_add_dev(parent, dp, if_typename, desc, diskid,
				       info.start, part, NULL);
		if (ret != EFI_SUCCESS) {
			printf("Adding partition %s failed\n", pdevname);
			continue;
		}
		disks++;
	}

	return disks;
}
#else
static int efi_disk_create_common(efi_handle_t handle,
				  struct efi_disk_obj *disk,
				  struct blk_desc *desc)
{
	efi_status_t ret;

	/* Hook up to the device list */
	efi_add_handle(handle);

	/* Fill in EFI IO Media info (for read/write callbacks) */
	disk->media.removable_media = desc->removable;
	disk->media.media_present = 1;
	disk->media.block_size = desc->blksz;
	disk->media.io_align = desc->blksz;
	disk->media.last_block = desc->lba - disk->offset;
	disk->ops.media = &disk->media;

	/* add protocols */
	disk->ops = block_io_disk_template;
	ret = efi_add_protocol(handle, &efi_block_io_guid, &disk->ops);
	if (ret != EFI_SUCCESS)
		goto err;

	ret = efi_add_protocol(handle, &efi_guid_device_path, disk->dp);
	if (ret != EFI_SUCCESS)
		goto err;

	return 0;

err:
	/* FIXME: undo adding protocols */
	return -1;
}

/*
 * Create a handle for a raw disk
 *
 * @dev		uclass device
 * @return	0 on success, -1 otherwise
 */
int efi_disk_create_raw(struct udevice *dev)
{
	struct blk_desc *desc = dev_get_uclass_platdata(dev);
	struct efi_disk_obj *disk = &desc->efi_disk;

	/* Don't add empty devices */
	if (!desc->lba)
		return -1;

	/* raw block device */
	disk->offset = 0;
	disk->part = 0;
	disk->dp = efi_dp_from_part(desc, 0);

	/* efi IO media */
	disk->media.logical_partition = 0;

	return efi_disk_create_common(dev, disk, desc);
}

/*
 * Create a handle for a partition
 *
 * @dev		uclass device
 * @return	0 on success, -1 otherwise
 */
int efi_disk_create_part(struct udevice *dev)
{
	struct udevice *parent = dev->parent;
	struct blk_desc *desc = dev_get_uclass_platdata(parent);
	struct blk_desc *this;
	struct disk_part *pdata = dev_get_uclass_platdata(dev);
	struct efi_disk_obj *disk;
	struct efi_device_path *node;

	int ret;

	/* dummy block device */
	this = malloc(sizeof(*this));
	if (!this)
		return -1;
	disk = &this->efi_disk;

	/* logical disk partition */
	disk->offset = pdata->gpt_part_info.start;
	disk->part = pdata->partnum;

	node = efi_dp_part_node(desc, disk->part);
	disk->dp = efi_dp_append_node(desc->efi_disk.dp, node);
	efi_free_pool(node);

	/* efi IO media */
	disk->media.logical_partition = 1;

	ret = efi_disk_create_common(dev, disk, desc);
	if (ret)
		goto err;

	/* partition may support file system access */
	disk->volume = efi_simple_file_system(desc, disk->part, disk->dp);
	ret = efi_add_protocol(dev,
			       &efi_simple_file_system_protocol_guid,
			       disk->volume);
	if (ret != EFI_SUCCESS)
		goto err;

	return 0;

err:
	free(this);
	/* FIXME: undo create */

	return -1;
}

int efi_disk_create(struct udevice *dev)
{
	enum uclass_id id;

	id = device_get_uclass_id(dev);

	if (id == UCLASS_BLK)
		return efi_disk_create_raw(dev);
	else if (id == UCLASS_PARTITION)
		return efi_disk_create_part(dev);
	else
		return -1;
}
#endif /* CONFIG_BLK */

/*
 * U-Boot doesn't have a list of all online disk devices. So when running our
 * EFI payload, we scan through all of the potentially available ones and
 * store them in our object pool.
 *
 * TODO(sjg@chromium.org): Actually with CONFIG_BLK, U-Boot does have this.
 * Consider converting the code to look up devices as needed. The EFI device
 * could be a child of the UCLASS_BLK block device, perhaps.
 *
 * This gets called from do_bootefi_exec().
 */
efi_status_t efi_disk_register(void)
{
#ifndef CONFIG_BLK
	struct efi_disk_obj *disk;
	int disks = 0;
	efi_status_t ret;
	int i, if_type;

	/* Search for all available disk devices */
	for (if_type = 0; if_type < IF_TYPE_COUNT; if_type++) {
		const struct blk_driver *cur_drvr;
		const char *if_typename;

		cur_drvr = blk_driver_lookup_type(if_type);
		if (!cur_drvr)
			continue;

		if_typename = cur_drvr->if_typename;
		printf("Scanning disks on %s...\n", if_typename);
		for (i = 0; i < 4; i++) {
			struct blk_desc *desc;
			char devname[32] = { 0 }; /* dp->str is u16[32] long */

			desc = blk_get_devnum_by_type(if_type, i);
			if (!desc)
				continue;
			if (desc->type == DEV_TYPE_UNKNOWN)
				continue;

			snprintf(devname, sizeof(devname), "%s%d",
				 if_typename, i);

			/* Add block device for the full device */
			ret = efi_disk_add_dev(NULL, NULL, if_typename, desc,
					       i, 0, 0, &disk);
			if (ret == EFI_NOT_READY) {
				printf("Disk %s not ready\n", devname);
				continue;
			}
			if (ret) {
				printf("ERROR: failure to add disk device %s, r = %lu\n",
				       devname, ret & ~EFI_ERROR_MASK);
				return ret;
			}
			disks++;

			/* Partitions show up as block devices in EFI */
			disks += efi_disk_create_partitions
						(&disk->header, desc,
						 if_typename, i, devname);
		}
	}
	printf("Found %d disks\n", disks);
#endif

	return EFI_SUCCESS;
}

static int efi_disk_probe(struct udevice *dev)
{
	device_set_name(dev, "BLOCK_IO");

	return 0;
}

U_BOOT_DRIVER(efi_disk) = {
	.name = "efi_disk",
	.id = UCLASS_EFI_PROTOCOL,
	.probe = efi_disk_probe,
};
