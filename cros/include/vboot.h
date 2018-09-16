/*
 * Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#ifndef __VBOOT_H
#define __VBOOT_H

#include <cros_common.h>
#include <gbb_header.h>
#include <vboot_flag.h>
#include <vboot_api.h>
#include <cros/cros_ofnode.h>

#define ID_LEN		256

/* Required alignment for struct vb2_context */
#define VBOOT_CONTEXT_ALIGN	16

/*
#include <inttypes.h>
*/

/* Magic number in the vboot persist header */
#define VBOOT_PERSIST_MAGIC	0xfeed1a3b

/**
 * Header for the information that persists in SRAM.
 *
 * This is set up on boot by the RO U-Boot, with the flags set to 0.
 * When an RW SPL fails to verify U-Boot, it sets a flag indicating this and
 * reboots. At that point RO U-Boot can see which firmware has been tried, in
 * VbExHashFirmwareBody(), and fail it before irrevocably jumping to its SPL
 * and then needing to reboot again (and again...).
 *
 * The information is cleared in RW U-Boot, since by then we know we have
 * succeeded in loading our RW firmware. So it only persists across reboot in
 * the case where we are failing. This will happen if the firmware updater
 * updates SPL but does not get around to updating the associated U-Boot.
 */
struct vboot_persist {
	uint32_t magic;		/* VBOOT_PERSIST_MAGIC */
	uint32_t flags;		/* enum vboot_persist_flags_t */
};

/* Signature for the stashed RW SPL */
#define VBOOT_SPL_SIGNATURE	0xf005ba11

/* Header for a stashed RW SPL */
struct vboot_spl_hdr {
	uint32_t signature;	/* VBOOT_SPL_SIGNATURE */
	uint32_t size;		/* Size excluding header */
	uint32_t crc32;		/* crc32 of contents */
	uint32_t spare;		/* Spare word */
};

/**
 * Information about each firmware type. We expect to have read-only (used for
 * RO-normal if enabled), read-write A, read-write B and recovery. Recovery
 * is the same as RO-normal unless EFS is enabled, in which case RO-normal
 * is a small, low-feature version incapable of running recovery, and we
 * have a separate recovery image.
 *
 * @vblock: Pointer to the vblock if loaded - this is NULL except for RW-A and
 *	RW-B
 * @size: Size of firmware in bytes (this is the compressed size if the
 *	firmware is compressed)
 * @cache: Firmware data, if loaded
 * @uncomp_size: Uncompressed size of firmware. Same as @size if it is not
 *	compressed
 * @fw_entry: Pointer to the firmware entry in the fmap - there are three
 *	possible ones: RO, RW-A and RW-B. Note that RO includes recovery if
 *	this is a separate U-Boot from the RO U-Boot.
 * @entry: Pointer to the firmware entry that we plan to load and run.
 *	Normally this is U-Boot, but with EFS it is SPL, since it is the SPL
 *	that is signed by the signer, verified by vboot and jumped to by
 *	RO U-Boot.
 */
struct vboot_fw_info {
	void *vblock;
	uint32_t size;
	void *cache;
	size_t uncomp_size;
	struct fmap_firmware_entry *fw_entry;
	struct fmap_entry *entry;
};

/*
 * @spl_entry: used for the verstage to return the location of the selected
 *	SPL slot
 * @u_boot_entry: used for the verstage to return the location of the selected
 *	U-Boot slot
 */
struct vboot_blob {
	struct fmap_entry spl_entry;
	struct fmap_entry u_boot_entry;
	struct vb2_context ctx __attribute__ ((aligned (VBOOT_CONTEXT_ALIGN)));
};

/*
 * The vboot_handoff structure contains the data to be consumed by downstream
 * firmware after firmware selection has been completed. Namely it provides
 * vboot shared data as well as the flags from VbInit.
 */
struct vboot_handoff {
	VbInitParams init_params;
	uint32_t selected_firmware;
	char shared_data[VB_SHARED_DATA_MIN_SIZE];
} __packed;

/**
 * Main verified boot data structure
 *
 * @blob: Persistent blob in the bloblist
 * @ctx: vboot2 API context
 * @nvdata_dev: Device to use to access non-volatile data
 * @kparams: Kernel params passed to Vboot library
 * @cros_ec: Chrome OS EC, or NULL if none
 * @gbb_flags: Copy of the flags from the Google Binary Block (GBB)
 * @tpm: TPM device
 * @video: Video device
 * @console: Video console (text device)
 * @panel: Display panel (can be NULL if there is none)
 * @config: Config node containing general configuation info
 *

 * @cparams: Common params passed to Vboot library
 * @vb2_return_code: Vboot library error, if any
 * @valid: false if this structure is not yet set up, true if it is
 * @fw_size: Size of firmware image in bytes - this starts off as the number
 *	of bytes in the section containing the firmware, but may be smaller if
 *	the vblock indicates that not all of that data was signed.
 * @readonly_firmware_id: Firmware ID read from RO firmware
 * @firmware_id: Firmware ID of selected RO/RW firmware
 */
struct vboot_info {
	struct vboot_blob *blob;
	struct vb2_context *ctx;
	struct udevice *nvdata_dev;
	struct udevice *cros_ec;
	uint32_t gbb_flags;
	struct udevice *tpm;
	struct udevice *video;
	struct udevice *console;
	struct udevice *panel;
	ofnode config;

	struct vboot_handoff *handoff;
	struct twostop_fmap fmap;
	struct udevice *fwstore;
	uint32_t vboot_out_flags;
#ifndef CONFIG_SPL_BUILD
	VbSelectAndLoadKernelParams kparams;
	VbCommonParams cparams;
#endif
	bool valid;
	enum vb2_return_code vb2_return_code;

	enum VbErrorPredefined_t vb_error;
	uint32_t fw_size;

	char readonly_firmware_id[ID_LEN];
	char firmware_id[ID_LEN];
	struct spl_image_info *spl_image;
};

#define vboot_log(level, fmt...) \
	log(LOGC_VBOOT, level, ##fmt)

static inline struct vboot_info *ctx_to_vboot(struct vb2_context *ctx)
{
	return ctx->non_vboot_context;
}

static inline struct vb2_context *vboot_get_ctx(struct vboot_info *vboot)
{
	return vboot->ctx;
}

/**
 * Set up the common params for the vboot library
 *
 * @vboot: Pointer to vboot structure
 * @cparams: Pointer to params structure to set up
 */
void vboot_init_cparams(struct vboot_info *vboot, VbCommonParams *cparams);

/**
 * Tell the EC to reboot and start up in RO.
 *
 * In recovery mode we need the EC to be in RO, so this function ensures that
 * it is. It requires rebooting the AP also.
 */
int vboot_request_ec_reboot_to_ro(void);

/**
 * Make a note of an error in the verified boot processs
 *
 * @vboot: Pointer to vboot structure
 * @stage: Name of vboot stage whic hfailed
 * @err: Number of error that occurred
 * @return -1 (always, so that caller can return it)
 */
int vboot_set_error(struct vboot_info *vboot, const char *stage,
		    enum VbErrorPredefined_t err);

/**
 * Read the vboot data from an FDT
 *
 * This is used in RW U-Boot to read state left behind by RO U-Boot
 *
 * @vboot: Pointer to vboot structure
 * @blob: Pointer to device tree blob containing the data
 */
int vboot_read_from_fdt(struct vboot_info *vboot, const void *blob);

/**
 * Write the vboot data to the FDT
 *
 * RO U-Boot uses this function to write out data for use by RW U-Boot. It
 * is also used to write out data to pass to the kernel.
 *
 * @vboot: Pointer to vboot structure
 * @blob: Pointer to device tree to update
 * @return 0 if OK, -ve on error
 */
int vboot_write_to_fdt(const struct vboot_info *vboot, void *blob);

/**
 * Update ACPI data
 *
 * For x86 systems, this writes a basic level of information in binary to
 * the ACPI tables for use by the kernel.
 *
 * @vboot: Pointer to vboot structure
 * @return 0 if OK, -ve on error
 */
int vboot_update_acpi(struct vboot_info *vboot);

/**
 * Dump vboot status information to the console
 *
 * @vboot: Pointer to vboot structure
 */
int vboot_dump(struct vboot_info *vboot);

/**
 * Get a pointer to the vboot structure
 *
 * @vboot: Pointer to vboot structure, if valid, else NULL
 */
struct vboot_info *vboot_get(void);

/**
 * Get a pointer to the vboot structure
 *
 * @vboot: Pointer to vboot structure (there is only one)
 */
struct vboot_info *vboot_get_nocheck(void);

/*
 * For the functions below, there are three combinations (thanks clchiou@):
 *
 * CONFIG_CROS_LEGACY_VBOOT defined and vboot->legacy_vboot == true
 * CONFIG_CROS_LEGACY_VBOOT defined and vboot->legacy_vboot == false
 * CONFIG_CROS_LEGACY_VBOOT is not defined
 *
 * The purpose of the first two cases is to allow the legacy and the new
 * vboot code paths to both be present in U-Boot so that U-Boot may run
 * either legacy or new vboot.
 *
 * Ultimately we will remove the legacy vboot, but in the meantime this
 * ability to boot either is valuable for testing and verification.
 *
 * The functions below allow the compiler/linker to optimize away the
 * legacy code when it is not needed.
 */

/**
 * @return true if we are running in legacy mode (vboot_twostop)
 */
static inline bool vboot_is_legacy(void)
{
#ifdef CONFIG_CROS_LEGACY_VBOOT
	struct vboot_info *vboot = vboot_get_nocheck();

	return vboot->legacy_vboot;
#else
	return false;
#endif
}

/**
 * Set whether we are in legacy mode or not
 *
 * @legacy: Set legacy mode to true/false
 */
static inline void vboot_set_legacy(bool legacy)
{
#ifdef CONFIG_CROS_LEGACY_VBOOT
	struct vboot_info *vboot = vboot_get_nocheck();

	vboot->legacy_vboot = legacy;
#endif
}

/**
 * Run the legacy vboot_twostop command
 *
 * @return 0 if OK, -ve on error
 */
int run_legacy_vboot_twostop(void);

/**
 * Load configuration for vboot, to control how it operates.
 *
 * @blob: Device tree blob containing the '/chromeos-config' node
 * @vboot: Pointer to vboot structure to update
 */
int vboot_load_config(const void *blob, struct vboot_info *vboot);

/**
 * Dump out information about the vboot persist information to the console
 *
 * @name: Name of this dump (for tracing)
 * @vboot: Pointer to vboot structure to dump
 */
void vboot_persist_dump(const char *name, struct vboot_info *vboot);

/**
 * Clear the vboot persist region
 *
 * Clear out any data in the persist region
 *
 * @vboot: Pointer to vboot structure
 */
void vboot_persist_clear(struct vboot_info *vboot);

/**
 * Set up the persist region if it does not already exist
 *
 * If there is a persist region, return it. If not, create it and then
 * return it.
 *
 * @blob: Pointer to device tree block containing config information
 * @vboot: Pointer to vboot structure
 */
int vboot_persist_init(const void *blob, struct vboot_info *vboot);

/*
 * vboot_save_hash() - save a hash to a secure location
 *
 * This can be verified in the resume path.
 *
 * @return 0 on success, -ve on error.
 */
int vboot_save_hash(void *digest, size_t digest_size);

/*
 * vboot_retrieve_hash() - get a previously saved hash digest
 *
 * @return 0 on success, -ve on error.
 */
int vboot_retrieve_hash(void *digest, size_t digest_size);

/**
 * vboot_platform_is_resuming() - check if we are resuming from suspend
 *
 * Determine if the platform is resuming from suspend
 *
 * @return 0 when not resuming, > 0 if resuming, < 0 on error.
 */
int vboot_platform_is_resuming(void);

int vboot_is_slot_a(struct vboot_info *vboot);

const char *vboot_slot_name(struct vboot_info *vboot);

void vboot_set_selected_region(struct vboot_info *vboot,
			       const struct fmap_entry *spl,
			       const struct fmap_entry *u_boot);

int fwstore_jump(struct vboot_info *vboot, int offset, int size);

int vboot_wants_oprom(struct vboot_info *vboot);
int resource_read(struct vboot_info *vboot, enum vb2_resource_index index,
		  uint32_t offset, void *buf, uint32_t size);
bool vboot_config_bool(struct vboot_info *vboot, const char *prop);
int vboot_fill_handoff(struct vboot_info *vboot);

int vboot_draw_screen(uint32_t screen, uint32_t locale);
u32 vboot_extend_pcr(struct vboot_info *vboot, int pcr,
		     enum vb2_pcr_digest which_digest);
uint32_t extend_pcrs(struct vboot_info *vboot);
int vboot_keymap_init(void);
int vboot_alloc(struct vboot_info **vbootp);
struct vboot_info *vboot_get_alloc(void);

#endif
