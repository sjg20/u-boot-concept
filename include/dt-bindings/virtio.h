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

#define VIRTIO_ID_NET		1 /* virtio net */
#define VIRTIO_ID_BLOCK		2 /* virtio block */
#define VIRTIO_ID_RNG		4 /* virtio rng */
#define VIRTIO_ID_MAX_NUM	27
#define VIRTIO_ID_FS		26 /* virtio filesystem */

#endif
