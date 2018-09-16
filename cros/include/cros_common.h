/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#ifndef CHROMEOS_COMMON_H_
#define CHROMEOS_COMMON_H_

#include <vb2_api.h>

#if 0
#if defined VBOOT_DEBUG
#define VB2_DEBUG(fmt, args...) \
	VB2_DEBUG("%s: %s: " fmt, __FILE__, __func__, ##args)
#elif defined DEBUG
#define VB2_DEBUG debug
#else
#define VB2_DEBUG(fmt, args...)
#endif

/*
 * VB2_DEBUG(), which indirectly calls printf(), has an internal buffer on output
 * string, and so cannot output very long string. Thus, if you want to print a
 * very long string, please use VB2_DEBUG_PUTS(), which calls puts().
 */
#if defined VBOOT_DEBUG || defined DEBUG
#define VB2_DEBUG_PUTS(str) puts(str)
#else
#define VB2_DEBUG_PUTS(str)
#endif
#endif

enum {
	BOOTSTAGE_VBOOT_FIRST = BOOTSTAGE_ID_USER,

	BOOTSTAGE_VBOOT_START = BOOTSTAGE_VBOOT_FIRST,
	BOOTSTAGE_VBOOT_START_TPMINIT,
	BOOTSTAGE_VBOOT_END_TPMINIT,
	BOOTSTAGE_VBOOT_START_VERIFY_SLOT,
	BOOTSTAGE_VBOOT_END_VERIFY_SLOT,
	BOOTSTAGE_VBOOT_START_HASH_BODY,
	BOOTSTAGE_VBOOT_END_HASH_BODY,
	BOOTSTAGE_VBOOT_DONE_HASHING,
	BOOTSTAGE_VBOOT_START_TPMPCR,
	BOOTSTAGE_VBOOT_END_TPMPCR,
	BOOTSTAGE_VBOOT_START_TPMLOCK,
	BOOTSTAGE_VBOOT_END_TPMLOCK,
	BOOTSTAMP_VBOOT_EC_DONE,
	BOOTSTAGE_VBOOT_END,
	BOOTSTAGE_VBOOT_DONE,

	/*
	 * Each vboot stage has a bootstage record starting at
	 * BOOTSTAGE_VBOOT_FIRST.
	 */
	BOOTSTAGE_ACCUM_VBOOT_BOOT_DEVICE_INFO,
	BOOTSTAGE_ACCUM_VBOOT_BOOT_DEVICE_READ,
	BOOTSTAGE_ACCUM_VBOOT_FIRMWARE_READ,

	BOOTSTAGE_VBOOT_LAST,
};

/* extra flags for enum tpm_return_code used by vboot */
enum {
	TPM_E_ALREADY_INITIALIZED	= 0x5000,
	TPM_E_INTERNAL_INCONSISTENCY,
	TPM_E_MUST_REBOOT,
	TPM_E_CORRUPTED_STATE,
	TPM_E_COMMUNICATION_ERROR,
	TPM_E_RESPONSE_TOO_LARGE,
	TPM_E_NO_DEVICE,
	TPM_E_INPUT_TOO_SMALL,
	TPM_E_WRITE_FAILURE,
	TPM_E_READ_EMPTY,
	TPM_E_READ_FAILURE,
	TPM_E_NV_DEFINED,
};

struct cros_ec_dev;

/**
 * Allocate a memory space aligned to cache line size.
 *
 * @param n	Size to be allocated
 * @return pointer to the allocated space or NULL on error.
 */
void *cros_memalign_cache(size_t n);

/* this function is implemented along with vb2_api */
int display_clear(void);

/**
 * Test code for performing a software sync
 *
 * This is used for dogfood devices where we want to update the RO EC.
 *
 * @param dev		cros_ec device
 * @param region_mask	Bit mask of regions to update:
 *				1 << EC_FLASH_REGION_RO: read-only
 *				1 << EC_FLASH_REGION_RW: read-write
 * @param force		Force update without checking existing contents
 * @param verify	Verify EC contents after writing
 */
int cros_test_swsync(struct cros_ec_dev *dev, int region_mask, int force,
		     int verify);

/**
 * Select a byte of the EC image to corrupt
 *
 * Next time verified boot calls VbExEcGetExpectedRW we will corrupt a single
 * byte of the image.
 *
 * @param offset	Offset to corrupt (-1 for none)
 * @param byte		Byte value to put into that offset
 */
void cros_ec_set_corrupt_image(int offset, int byte);

/**
 * Ensure that bitmaps are loaded into our gbb area
 *
 * @return 0 if ok, -1 on error
 */
int cros_cboot_twostop_read_bmp_block(void);

#endif /* CHROMEOS_COMMON_H_ */
