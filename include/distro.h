/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __distro_h
#define __distro_h

struct blk_desc;

/**
 * distro_boot_setup() - Set up a bootflow for distro boot from a block device
 *
 * This fills out a bootflow for a particular boot device and partition. It
 * scans for a filesystem and suitable file, updating the bootflow accordingly.
 *
 * This sets the following fields in @bflow:
 *
 *	type, size, fname, state, subdir, buf
 *
 * The caller mast have already set the other fields.
 *
 * @desc: Block-device descriptor
 * @partnum: Partition number (1..)
 * @bflow: Partial bootflow to be completed by this function
 * @return 0 on success (bootflow got to 'loaded' state), -ve on error
 */
int distro_boot_setup(struct blk_desc *desc, int partnum,
		      struct bootflow *bflow);

/**
 * distro_boot_setup() - Set up a bootflow for distro boot from a network device
 *
 * This fills out a bootflow for a network device. It scans the tftp server for
 * a suitable file, updating the bootflow accordingly.
 *
 * At present no control is provided as to which network device is used.
 *
 * This sets the following fields in @bflow:
 *
 *	type, size, fname, state,, buf
 *
 * The caller mast have already set the other fields.
 *
 * @bflow: Partial bootflow to be completed by this function
 * @return 0 on success (bootflow got to 'loaded' state), -ve on error
 */
int distro_net_setup(struct bootflow *bflow);

/**
 * distro_boot() - Boot a distro
 *
 * Boots a bootflow of type BOOTFLOWT_DISTRO. This typically needs to load more
 * files as it processes and this is done via the same media as the bootflow
 * was loaded
 *
 * @bflow: Bootflow to boot
*/
int distro_boot(struct bootflow *bflow);

#endif
