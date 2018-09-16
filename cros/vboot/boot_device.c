/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#include <common.h>
#include <bootstage.h>
#include <dm.h>
#include <part.h>
#include <usb.h>
#include <cros/cros_common.h>
#include <cros/vboot.h>

/* Maximum number of devices we can support */
enum {
	MAX_DISK_INFO	= 10,
};

/* TODO Move these definitions to vboot_wrapper.h or somewhere like that. */
enum {
	VBERROR_DISK_NO_DEVICE = 1,
	VBERROR_DISK_OUT_OF_RANGE,
	VBERROR_DISK_READ_ERROR,
	VBERROR_DISK_WRITE_ERROR,
};

static int usb_is_enumerated;

/**
 * Add a device to info array if it matches the supplied disk_flags
 *
 * @param name		Peripheral name
 * @param dev		Device to check
 * @param req_flags	Requested flags which must be present for each device
 * @param info		Output array of matching devices
 * @return 0 if added, -ENOENT if not
 */
static int add_matching_device(struct udevice *dev, uint32_t req_flags,
			       VbDiskInfo *info)
{
	struct blk_desc *bdev = dev_get_uclass_platdata(dev);
	uint32_t flags;

	/* Ignore zero-length devices */
	if (!bdev->lba)
		return -ENOENT;

	/*
	 * Only add this storage device if the properties of req_flags is a
	 * subset of the properties of flags.
	 */
	flags = bdev->removable ? VB_DISK_FLAG_REMOVABLE : VB_DISK_FLAG_FIXED;
	if ((flags & req_flags) != req_flags)
		return -ENOENT;

	info->handle = (VbExDiskHandle_t)dev;
	info->bytes_per_lba = bdev->blksz;
	info->lba_count = bdev->lba;
	info->flags = flags | VB_DISK_FLAG_EXTERNAL_GPT;
	info->name = dev->name;

	return 0;
}

static int boot_device_usb_start(void)
{
	int enumerate = 1;

	/*
	 * if the USB devices have already been enumerated, redo it
	 * only if something has been plugged on unplugged.
	 */
	if (usb_is_enumerated)
		enumerate = usb_detect_change();

	if (enumerate) {
		/*
		 * We should stop all USB devices first. Otherwise we can't
		 * detect any new devices.
		 */
		usb_stop();

		if (usb_init() >= 0)
			usb_is_enumerated = 1;
	}

	return 0;
}

VbError_t VbExDiskGetInfo(VbDiskInfo **infos_ptr, uint32_t *count_ptr,
			  uint32_t disk_flags)
{
	VbDiskInfo *infos;
	uint32_t max_count;	/* maximum devices to scan for */
	uint32_t count = 0;	/* number of matching devices found */
	struct udevice *dev;

	/* We return as many disk infos as possible. */
	max_count = MAX_DISK_INFO;

	infos = calloc(max_count, sizeof(VbDiskInfo));

	bootstage_start(BOOTSTAGE_ACCUM_VBOOT_BOOT_DEVICE_INFO,
			"boot_device_info");

	/* If we are looking for removable disks, scan USB */
	if (disk_flags & VB_DISK_FLAG_REMOVABLE)
		boot_device_usb_start();

	/* Scan through all the interfaces looking for devices */
	for (uclass_first_device(UCLASS_BLK, &dev);
	     dev;
	     uclass_next_device(&dev)) {
		/* Now record the devices that have the required flags */
		if (!add_matching_device(dev, disk_flags, infos + count))
			count++;
		if (count == max_count) {
			vboot_log(LOGL_WARNING,
				  "Reached maximum device count\n");
		}
	}

	if (count) {
		*infos_ptr = infos;
		*count_ptr = count;
	} else {
		*infos_ptr = NULL;
		*count_ptr = 0;
		free(infos);
	}
	bootstage_accum(BOOTSTAGE_ACCUM_VBOOT_BOOT_DEVICE_INFO);

	/* The operation itself succeeds, despite scan failure all about */
	return VBERROR_SUCCESS;
}

VbError_t VbExDiskFreeInfo(VbDiskInfo *infos, VbExDiskHandle_t preserve_handle)
{
	/* We do nothing for preserve_handle as we keep all the devices on. */
	free(infos);
	return VBERROR_SUCCESS;
}

VbError_t VbExDiskRead(VbExDiskHandle_t handle, uint64_t lba_start,
                       uint64_t lba_count, void *buffer)
{
	struct udevice *dev = (struct udevice *)handle;
	struct blk_desc *bdev = dev_get_uclass_platdata(dev);
	uint64_t blks_read;

        VB2_DEBUG("lba_start=%u, lba_count=%u\n",
		(unsigned)lba_start, (unsigned)lba_count);

	if (lba_start >= bdev->lba || lba_start + lba_count > bdev->lba)
		return VBERROR_DISK_OUT_OF_RANGE;

	bootstage_start(BOOTSTAGE_ACCUM_VBOOT_BOOT_DEVICE_READ,
			"boot_device_read");
	blks_read = blk_dread(bdev, lba_start, lba_count, buffer);
	bootstage_accum(BOOTSTAGE_ACCUM_VBOOT_BOOT_DEVICE_READ);
	if (blks_read != lba_count)
		return VBERROR_DISK_READ_ERROR;

	return VBERROR_SUCCESS;
}

VbError_t VbExDiskWrite(VbExDiskHandle_t handle, uint64_t lba_start,
                        uint64_t lba_count, const void *buffer)
{
	struct udevice *dev = (struct udevice *)handle;
	struct blk_desc *bdev = dev_get_uclass_platdata(dev);

	if (lba_start >= bdev->lba || lba_start + lba_count > bdev->lba)
		return VBERROR_DISK_OUT_OF_RANGE;

	if (blk_dwrite(bdev, lba_start, lba_count, buffer) != lba_count)
		return VBERROR_DISK_WRITE_ERROR;

	return VBERROR_SUCCESS;
}

/*
 * Simple implementation of new streaming APIs.  This turns them into calls to
 * the sector-based disk read/write functions above.  This will be replaced
 * imminently with fancier streaming.  In the meantime, this will allow the
 * vboot_reference change which uses the streaming APIs to commit.
 */

/* The stub implementation assumes 512-byte disk sectors */
#define LBA_BYTES 512

/* Internal struct to simulate a stream for sector-based disks */
struct disk_stream {
	/* Disk handle */
	VbExDiskHandle_t handle;

	/* Next sector to read */
	uint64_t sector;

	/* Number of sectors left in partition */
	uint64_t sectors_left;
};

VbError_t VbExStreamOpen(VbExDiskHandle_t handle, uint64_t lba_start,
			 uint64_t lba_count, VbExStream_t *stream)
{
	struct disk_stream *s;

	if (!handle) {
		*stream = NULL;
		return VBERROR_UNKNOWN;
	}

	s = malloc(sizeof(*s));
	s->handle = handle;
	s->sector = lba_start;
	s->sectors_left = lba_count;

	*stream = (void *)s;

	return VBERROR_SUCCESS;
}

VbError_t VbExStreamRead(VbExStream_t stream, uint32_t bytes, void *buffer)
{
	struct disk_stream *s = (struct disk_stream *)stream;
	uint64_t sectors;
	VbError_t rv;

	if (!s)
		return VBERROR_UNKNOWN;

	/* For now, require reads to be a multiple of the LBA size */
	if (bytes % LBA_BYTES)
		return VBERROR_UNKNOWN;

	/* Fail on overflow */
	sectors = bytes / LBA_BYTES;
	if (sectors > s->sectors_left)
		return VBERROR_UNKNOWN;

	rv = VbExDiskRead(s->handle, s->sector, sectors, buffer);
	if (rv != VBERROR_SUCCESS)
		return rv;

	s->sector += sectors;
	s->sectors_left -= sectors;

	return VBERROR_SUCCESS;
}

void VbExStreamClose(VbExStream_t stream)
{
	struct disk_stream *s = (struct disk_stream *)stream;

	free(s);
}
