/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __bootdevice_efi_h
#define __bootdevice_efi_h

struct blk_desc;

/**
 * efiloader_boot_setup() - Set up a bootflow for EFI boot from a block device
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
int efiloader_boot_setup(struct blk_desc *desc, int partnum,
			 struct bootflow *bflow);

/**
 * efiloader_boot() - Boot an EFI binary
 *
 * Boots a bootflow of type BOOTFLOWT_EFI_LOADER. This boots an EFI application
 * which takes care of the boot from then on.
 *
 * @bflow: Bootflow to boot
 */
int efiloader_boot(struct bootflow *bflow);

#endif
