/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2018, Tuomas Tynkkynen <tuomas.tynkkynen@iki.fi>
 * Copyright (C) 2018, Bin Meng <bmeng.cn@gmail.com>
 *
 * Binding file for virtio IDs
 *
 * This file is largely based on Linux kernel virtio_*.h files
 */

#ifndef __DT_BINDINGS_VIRTIO
#define __DT_BINDINGS_VIRTIO

#define VIRTIO_ID_NET		0x1
#define VIRTIO_ID_BLOCK		0x2
#define VIRTIO_ID_CONSOLE	0x3
#define VIRTIO_ID_RNG		0x4
#define VIRTIO_ID_BALLOON	0x5
#define VIRTIO_ID_IOMEM		0x6
#define VIRTIO_ID_RPMSG		0x7
#define VIRTIO_ID_SCSI		0x8
#define VIRTIO_ID_9P		0x9
#define VIRTIO_ID_MAC80211_WLAN	0xa
#define VIRTIO_ID_RPROC_SERIAL	0xb
#define VIRTIO_ID_CAIF		0xc
#define VIRTIO_ID_MEMORY_BALLOON 0xd
#define VIRTIO_ID_GPU		0x10
#define VIRTIO_ID_INPUT		0x12
#define VIRTIO_ID_VSOCK		0x13
#define VIRTIO_ID_CRYPTO	0x14
#define VIRTIO_ID_I2C		0x16
#define VIRTIO_ID_FS		0x1a
#define VIRTIO_ID_PMEM		0x1b
#define VIRTIO_ID_MAC80211_HWSIM 0x1d
#define VIRTIO_ID_VIDENC	0x1e
#define VIRTIO_ID_VIDDEC	0x1f
#define VIRTIO_ID_SCMI		0x20
#define VIRTIO_ID_SND		0x21
#define VIRTIO_ID_MAX_NUM	0x22

#endif
