/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * Taken from https://chromium.googlesource.com/chromiumos/platform/vboot
 *
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Size of non-volatile data used by vboot.
 *
 * If you only support non-volatile data format V1, then use VB2_NVDATA_SIZE.
 * If you support V2, use VB2_NVDATA_SIZE_V2 and set context flag
 * VB2_CONTEXT_NVDATA_V2.
 */
#define VB2_NVDATA_SIZE 16
#define VB2_NVDATA_SIZE_V2 64

/* Size of secure data spaces used by vboot */
#define VB2_SECDATA_FIRMWARE_SIZE 10
#define VB2_SECDATA_KERNEL_SIZE_V02 13
#define VB2_SECDATA_KERNEL_SIZE_V10 40
#define VB2_SECDATA_KERNEL_MIN_SIZE 13
#define VB2_SECDATA_KERNEL_MAX_SIZE 64
#define VB2_SECDATA_FWMP_MIN_SIZE 40
#define VB2_SECDATA_FWMP_MAX_SIZE 64

/* Helper for aligning fields in vb2_context. */
#define VB2_PAD_STRUCT3(size, align, count) \
	u8 _pad##count[align - (((size - 1) % align) + 1)]
#define VB2_PAD_STRUCT2(size, align, count) VB2_PAD_STRUCT3(size, align, count)
#define VB2_PAD_STRUCT(size, align) VB2_PAD_STRUCT2(size, align, __COUNTER__)

/* MAX_SIZE should not be changed without bumping up DATA_VERSION_MAJOR. */
#define VB2_CONTEXT_MAX_SIZE 384

/*
 * Context for firmware verification.  Pass this to all vboot APIs.
 *
 * Context is stored as part of vb2_shared_data, initialized with vb2api_init().
 * Subsequent retrieval of the context object should be done by calling
 * vb2api_reinit(), e.g. if switching firmware applications.
 *
 * The context struct can be seen as the "publicly accessible" portion of
 * vb2_shared_data, and thus does not require its own magic and version fields.
 */
struct vb2_context {
	/**********************************************************************
	 * Fields caller must initialize before calling any API functions.
	 */

	/*
	 * Flags; see vb2_context_flags.  Some flags may only be set by caller
	 * prior to calling vboot functions.
	 */
	u64 flags;

	/*
	 * Non-volatile data.  Caller must fill this from some non-volatile
	 * location before calling vb2api_fw_phase1.  If the
	 * VB2_CONTEXT_NVDATA_CHANGED flag is set when a vb2api function
	 * returns, caller must save the data back to the non-volatile location
	 * and then clear the flag.
	 */
	u8 nvdata[VB2_NVDATA_SIZE_V2];
	VB2_PAD_STRUCT(VB2_NVDATA_SIZE_V2, 8);

	/*
	 * Secure data for firmware verification stage.  Caller must fill this
	 * from some secure non-volatile location before calling
	 * vb2api_fw_phase1.  If the VB2_CONTEXT_SECDATA_FIRMWARE_CHANGED flag
	 * is set when a function returns, caller must save the data back to the
	 * secure non-volatile location and then clear the flag.
	 */
	u8 secdata_firmware[VB2_SECDATA_FIRMWARE_SIZE];
	VB2_PAD_STRUCT(VB2_SECDATA_FIRMWARE_SIZE, 8);

	/**********************************************************************
	 * Fields caller must initialize before calling vb2api_kernel_phase1().
	 */

	/*
	 * Secure data for kernel verification stage.  Caller must fill this
	 * from some secure non-volatile location before calling
	 * vb2api_kernel_phase1.  If the VB2_CONTEXT_SECDATA_KERNEL_CHANGED
	 * flag is set when a function returns, caller must save the data back
	 * to the secure non-volatile location and then clear the flag.
	 */
	u8 secdata_kernel[VB2_SECDATA_KERNEL_MAX_SIZE];
	VB2_PAD_STRUCT(VB2_SECDATA_KERNEL_MAX_SIZE, 8);

	/*
	 * Firmware management parameters (FWMP) secure data.  Caller must fill
	 * this from some secure non-volatile location before calling
	 * vb2api_kernel_phase1.  Since FWMP is a variable-size space, caller
	 * should initially fill in VB2_SECDATA_FWMP_MIN_SIZE bytes, and call
	 * vb2_secdata_fwmp_check() to see whether more should be read.  If the
	 * VB2_CONTEXT_SECDATA_FWMP_CHANGED flag is set when a function
	 * returns, caller must save the data back to the secure non-volatile
	 * location and then clear the flag.
	 */
	u8 secdata_fwmp[VB2_SECDATA_FWMP_MAX_SIZE];
	VB2_PAD_STRUCT(VB2_SECDATA_FWMP_MAX_SIZE, 8);

	/*
	 * Context pointer for use by caller.  Verified boot never looks at
	 * this.  Put context here if you need it for APIs that verified boot
	 * may call (vb2ex_...() functions).
	 */
	void *non_vboot_context;
};

/*
 * Data shared between vboot API calls.  Stored at the start of the work
 * buffer.
 */
struct vb2_shared_data {
	/* Magic number for struct (VB2_SHARED_DATA_MAGIC) */
	u32 magic;

	/* Version of this structure */
	u16 struct_version_major;
	u16 struct_version_minor;

	/* Public fields are stored in the context object */
	struct vb2_context ctx;

	/* Padding for adding future vb2_context fields */
	u8 padding[VB2_CONTEXT_MAX_SIZE - sizeof(struct vb2_context)];

	/* Work buffer length in bytes. */
	u32 workbuf_size;

	/*
	 * Amount of work buffer used so far.  Verified boot sub-calls use
	 * this to know where the unused work area starts.
	 */
	u32 workbuf_used;

	/* Flags; see enum vb2_shared_data_flags */
	u32 flags;

	/*
	 * Reason we are in recovery mode this boot (enum vb2_nv_recovery), or
	 * 0 if we aren't.
	 */
	u32 recovery_reason;

	/* Firmware slot used last boot (0=A, 1=B) */
	u32 last_fw_slot;

	/* Result of last boot (enum vb2_fw_result) */
	u32 last_fw_result;

	/* Firmware slot used this boot */
	u32 fw_slot;

	/*
	 * Version for this slot (top 16 bits = key, lower 16 bits = firmware).
	 *
	 * TODO: Make this a union to allow getting/setting those versions
	 * separately?
	 */
	u32 fw_version;

	/* Version from secdata_firmware (must be <= fw_version to boot). */
	u32 fw_version_secdata;

	/*
	 * Status flags for this boot; see enum vb2_shared_data_status.  Status
	 * is "what we've done"; flags above are "decisions we've made".
	 */
	u32 status;

	/* Offset from start of this struct to GBB header */
	u32 gbb_offset;

	/**********************************************************************
	 * Data from kernel verification stage.
	 *
	 * TODO: shouldn't be part of the main struct, since that needlessly
	 * uses more memory during firmware verification.
	 */

	/*
	 * Version for the current kernel (top 16 bits = key, lower 16 bits =
	 * kernel preamble).
	 *
	 * TODO: Make this a union to allow getting/setting those versions
	 * separately?
	 */
	u32 kernel_version;

	/* Version from secdata_kernel (must be <= kernel_version to boot) */
	u32 kernel_version_secdata;

	/**********************************************************************
	 * Temporary variables used during firmware verification.  These don't
	 * really need to persist through to the OS, but there's nowhere else
	 * we can put them.
	 */

	/* Offset of preamble from start of vblock */
	u32 vblock_preamble_offset;

	/*
	 * Offset and size of packed data key in work buffer.  Size is 0 if
	 * data key is not stored in the work buffer.
	 */
	u32 data_key_offset;
	u32 data_key_size;

	/*
	 * Offset and size of firmware preamble in work buffer.  Size is 0 if
	 * preamble is not stored in the work buffer.
	 */
	u32 preamble_offset;
	u32 preamble_size;

	/*
	 * Offset and size of hash context in work buffer.  Size is 0 if
	 * hash context is not stored in the work buffer.
	 */
	u32 hash_offset;
	u32 hash_size;

	/*
	 * Current tag we're hashing
	 *
	 * For new structs, this is the offset of the vb2_signature struct
	 * in the work buffer.
	 *
	 * TODO: rename to hash_sig_offset when vboot1 structs are deprecated.
	 */
	u32 hash_tag;

	/* Amount of data we still expect to hash */
	u32 hash_remaining_size;

	/**********************************************************************
	 * Temporary variables used during kernel verification.  These don't
	 * really need to persist through to the OS, but there's nowhere else
	 * we can put them.
	 *
	 * TODO: make a union with the firmware verification temp variables,
	 * or make both of them workbuf-allocated sub-structs, so that we can
	 * overlap them so kernel variables don't bloat firmware verification
	 * stage memory requirements.
	 */

	/*
	 * Formerly a pointer to vboot1 shared data header ("VBSD").  Caller
	 * may now export a copy of VBSD via vb2api_export_vbsd().
	 * TODO: Remove this field and bump struct_version_major.
	 */
	uintptr_t reserved0;

	/*
	 * Offset and size of packed kernel key in work buffer.  Size is 0 if
	 * subkey is not stored in the work buffer.  Note that kernel key may
	 * be inside the firmware preamble.
	 */
	u32 kernel_key_offset;
	u32 kernel_key_size;
} __packed;
