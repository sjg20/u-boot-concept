/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Internal header file for virtio
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#ifndef _VIRTIO_INTERNAL_H
#define _VIRTIO_INTERNAL_H

struct udevice;

/* MMIO operations from virtio_mmcio.c */
extern const struct dm_virtio_ops virtio_mmio_ops;

/* exported probe function from virtio_mmcio.c */
int virtio_mmio_probe(struct udevice *udev);

#endif
