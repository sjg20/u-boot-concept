/*
 * Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Functions for querying, manipulating and locking rollback indices
 * stored in the TPM NVRAM.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ANTIROLLBACK_H_
#define ANTIROLLBACK_H_

struct vboot_info;
enum vb2_pcr_digest;

/* TPM NVRAM location indices. */
#define FIRMWARE_NV_INDEX               0x1007
#define KERNEL_NV_INDEX                 0x1008
/*
 * 0x1009 used to be used as a backup space. Think of conflicts if you
 * want to use 0x1009 for something else.
 */
#define BACKUP_NV_INDEX                 0x1009
#define FWMP_NV_INDEX                   0x100a
#define REC_HASH_NV_INDEX               0x100b
#define REC_HASH_NV_SIZE                VB2_SHA256_DIGEST_SIZE

/* Structure definitions for TPM spaces */

/* Flags for firmware space */

/*
 * Last boot was developer mode.  TPM ownership is cleared when transitioning
 * to/from developer mode.
 */
#define FLAG_LAST_BOOT_DEVELOPER 0x01

/* All functions return TPM_SUCCESS (zero) if successful, non-zero if error */

uint32_t antirollback_read_space_firmware(struct vboot_info *vboot);

/**
 * Write may be called if the versions change.
 */
uint32_t antirollback_write_space_firmware(struct vboot_info *vboot);

/**
 * Lock must be called.
 */
uint32_t antirollback_lock_space_firmware(void);

/* Read recovery hash data from TPM. */
uint32_t antirollback_read_space_rec_hash(uint8_t *data, uint32_t size);
/* Write new hash data to recovery space in TPM. */
uint32_t antirollback_write_space_rec_hash(const uint8_t *data, uint32_t size);
/* Lock down recovery hash space in TPM. */
uint32_t antirollback_lock_space_rec_hash(void);

/* Start of the root of trust */
uint32_t vboot_setup_tpm(struct vboot_info *vboot);

/* vboot_extend_pcr function for vb2 context */
uint32_t vboot_extend_pcr(struct vboot_info *vboot, int pcr,
			  enum vb2_pcr_digest which_digest);
u32 tpm_clear_and_reenable(void);
u32 factory_initialise_tpm(struct vboot_info *vboot);

#endif  /* ANTIROLLBACK_H_ */
