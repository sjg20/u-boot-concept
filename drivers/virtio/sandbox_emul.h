/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * VirtIO Sandbox emulator, for testing purpose only. This emulates the QEMU
 * side of virtio, using the MMIO driver and handling any accesses
 *
 * This handles traffic from the virtio_ring
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#ifndef __SANDBOX_EMUL_H
#define __SANDBOX_EMUL_H

#include "virtio_mmio.h"
#include "virtio_types.h"

enum sandboxio_size_t;
struct udevice;
struct vring_desc;

enum {
	MAX_VIRTIO_QUEUES	= 8,
	QUEUE_MAX_SIZE		= 256,
};

/**
 * struct virtio_emul_queue - Emulator's state for a single virtqueue
 */
struct virtio_emul_queue {
	__virtio32 num;
	__virtio32 ready;
	__virtio64 desc_addr;
	__virtio64 avail_addr;
	__virtio64 used_addr;
	__virtio16 last_avail_idx; // Device's internal counter
};

/**
 * struct sandbox_emul_priv - Private info for the emulator
 */
struct sandbox_emul_priv {
	struct virtio_mmio_priv mmio;
	int num_queues;
	int queue_sel;
	u32 status;
	u64 features_sel;
	u64 features;
	u64 driver_features;
	u32 interrupt_status;
	u32 config_generation;
	struct virtio_emul_queue queues[MAX_VIRTIO_QUEUES];
};

/**
 * struct virtio_emul_ops - Operations for a virtio device emulator
 *
 * @process_request:
 * @get_config: Reads from the device-specific configuration space
 * @get_features: Returns the device-specific feature bits
 */
struct virtio_emul_ops {
	/**
	 * process_request() - Handles a single request from the driver
	 *
	 * @dev: The emulator device
	 * @descs: Pointer to the virtqueue's descriptor table
	 * @head_idx: The index of the first descriptor in the chain for
	 *	this request
	 * @writtenp: Returns the total number of bytes written by the
	 *	device into the driver's buffers (e.g. for a read
	 *	request and the status byte). This is what will be
	 *	placed in the `len` field of the used ring element.
	 * @return 0 on success, negative on error.
	 */
	int (*process_request)(struct udevice *dev, struct vring_desc *descs,
			       u32 head_idx, int *writtenp);

	/**
	 * get_config() - Reads from the device-specific configuration space
	 *
	 * @dev: The emulator device
	 * @offset: The byte offset into the configuration space to read from
	 * @buf: The buffer to copy the configuration data into
	 * @size: The number of bytes to read
	 * @return 0 on success, negative on error.
	 */
	int (*get_config)(struct udevice *dev, ulong offset, void *buf,
			  enum sandboxio_size_t size);

	/**
	 * get_features() - Returns the device-specific feature bits
	 *
	 * @dev: The emulator device
	 * @return A bitmask of the device-specific features to be OR'd
	 *	with the transport features.
	 */
	u64 (*get_features)(struct udevice *dev);

	/**
	 * get_device_id() - Returns the virtio device ID
	 *
	 * @dev: The emulator device
	 * @return The virtio device ID for this emulator
	 */
	u32 (*get_device_id)(struct udevice *dev);
};

#define virtio_emul_get_ops(dev) ((struct virtio_emul_ops *)(dev)->driver->ops)

#endif
