/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 *
 * Functions for querying, manipulating and locking rollback indices
 * stored in the TPM NVRAM.
 *
 * Take from coreboot file of the same name
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <common.h>
#include <log.h>
#include <tpm-common.h>
#include <tpm-v1.h>
#include <cros/antirollback.h>
#include <cros/nvdata.h>
#include <cros/vboot.h>

// #include <security/tpm/tspi.h>

#ifndef offsetof
#define offsetof(A,B) __builtin_offsetof(A,B)
#endif

#define CHECK_RET(tpm_cmd) do {						\
	u32 result_;							\
									\
	result_ = (tpm_cmd);						\
	if (result_ != TPM_SUCCESS) {					\
		vboot_log(LOGL_ERR, "Antirollback: %08x returned by "	\
			#tpm_cmd "\n", (int)result_);			\
		return result_;						\
	}								\
} while (0)


u32 vboot_extend_pcr(struct vboot_info *vboot, int pcr,
		     enum vb2_pcr_digest which_digest)
{
	uint8_t buffer[VB2_PCR_DIGEST_RECOMMENDED_SIZE];
	uint8_t out[VB2_PCR_DIGEST_RECOMMENDED_SIZE];
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	u32 size = sizeof(buffer);
	int rv;

	rv = vb2api_get_pcr_digest(ctx, which_digest, buffer, &size);
	if (rv != VB2_SUCCESS)
		return rv;
	if (size < TPM_PCR_MINIMUM_DIGEST_SIZE)
		return VB2_ERROR_UNKNOWN;

	return tpm_extend(pcr, buffer, out);
}

uint32_t extend_pcrs(struct vboot_info *vboot)
{
	return vboot_extend_pcr(vboot, 0, BOOT_MODE_PCR) ||
	       vboot_extend_pcr(vboot, 1, HWID_DIGEST_PCR);
}

static u32 read_space_rec_hash(uint8_t *data)
{
	CHECK_RET(tpm_nv_read_value(REC_HASH_NV_INDEX, data, REC_HASH_NV_SIZE));

	return TPM_SUCCESS;
}

/*
 * This is derived from rollback_index.h of vboot_reference. see struct
 * RollbackSpaceKernel for details.
 */
static const uint8_t secdata_kernel[] = {
	0x02,
	0x4C, 0x57, 0x52, 0x47,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00,
	0xE8,
};

/*
 * This is used to initialise the TPM space for recovery hash after defining
 * it. Since there is no data available to calculate hash at the point where TPM
 * space is defined, initialise it to all 0s.
 */
static const uint8_t rec_hash_data[REC_HASH_NV_SIZE] = { };

#if IS_ENABLED(CONFIG_TPM2)
/*
 * Different sets of NVRAM space attributes apply to the "ro" spaces,
 * i.e. those which should not be possible to delete or modify once
 * the RO exits, and the rest of the NVRAM spaces.
 */
const static TPMA_NV ro_space_attributes = {
	.TPMA_NV_PPWRITE = 1,
	.TPMA_NV_AUTHREAD = 1,
	.TPMA_NV_PPREAD = 1,
	.TPMA_NV_PLATFORMCREATE = 1,
	.TPMA_NV_WRITE_STCLEAR = 1,
	.TPMA_NV_POLICY_DELETE = 1,
};

const static TPMA_NV rw_space_attributes = {
	.TPMA_NV_PPWRITE = 1,
	.TPMA_NV_AUTHREAD = 1,
	.TPMA_NV_PPREAD = 1,
	.TPMA_NV_PLATFORMCREATE = 1,
};

/*
 * This policy digest was obtained using TPM2_PolicyPCR
 * selecting only PCR_0 with a value of all zeros.
 */
const static uint8_t pcr0_unchanged_policy[] = {
	0x09, 0x93, 0x3C, 0xCE, 0xEB, 0xB4, 0x41, 0x11, 0x18, 0x81, 0x1D,
	0xD4, 0x47, 0x78, 0x80, 0x08, 0x88, 0x86, 0x62, 0x2D, 0xD7, 0x79,
	0x94, 0x46, 0x62, 0x26, 0x68, 0x8E, 0xEE, 0xE6, 0x6A, 0xA1};

/* Nothing special in the TPM2 path yet. */
static u32 safe_write(u32 index, const void *data, u32 length)
{
	return tlcl_write(index, data, length);
}

static u32 set_space(const char *name, u32 index, const void *data,
			  u32 length, const TPMA_NV nv_attributes,
			  const uint8_t *nv_policy, size_t nv_policy_size)
{
	u32 rv;

	rv = tlcl_define_space(index, length, nv_attributes, nv_policy,
			       nv_policy_size);
	if (rv == TPM_E_NV_DEFINED) {
		/*
		 * Continue with writing: it may be defined, but not written
		 * to. In that case a subsequent tlcl_read() would still return
		 * TPM_E_BADINDEX on TPM 2.0. The cases when some non-firmware
		 * space is defined while the firmware space is not there
		 * should be rare (interrupted initialization), so no big harm
		 * in writing once again even if it was written already.
		 */
		vboot_log(LOGL_DEBUG, "%s: %s space already exists\n", __func__, name);
		rv = TPM_SUCCESS;
	}

	if (rv != TPM_SUCCESS)
		return rv;

	return safe_write(index, data, length);
}

static u32 set_firmware_space(const void *firmware_blob)
{
	return set_space("firmware", FIRMWARE_NV_INDEX, firmware_blob,
			 VB2_SECDATA_SIZE, ro_space_attributes,
			 pcr0_unchanged_policy, sizeof(pcr0_unchanged_policy));
}

static u32 set_kernel_space(const void *kernel_blob)
{
	return set_space("kernel", KERNEL_NV_INDEX, kernel_blob,
			 sizeof(secdata_kernel), rw_space_attributes, NULL, 0);
}

static u32 set_rec_hash_space(const uint8_t *data)
{
	return set_space("MRC Hash", REC_HASH_NV_INDEX, data,
			 REC_HASH_NV_SIZE,
			 ro_space_attributes, pcr0_unchanged_policy,
			 sizeof(pcr0_unchanged_policy));
}

static u32 _factory_initialise_tpm(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);

	vboot_log(LOG_WARNING, "Setting up TPM for first time from factory\n");
	CHECK_RET(tlcl_force_clear());

	/*
	 * Of all NVRAM spaces defined by this function the firmware space
	 * must be defined last, because its existence is considered an
	 * indication that TPM factory initialization was successfully
	 * completed.
	 */
	CHECK_RET(set_kernel_space(secdata_kernel));

	if (IS_ENABLED(CONFIG_VBOOT_HAS_REC_HASH_SPACE))
		CHECK_RET(set_rec_hash_space(rec_hash_data));

	CHECK_RET(set_firmware_space(ctx->secdata));

	return TPM_SUCCESS;
}

u32 antirollback_lock_space_firmware(void)
{
	return tlcl_lock_nv_write(FIRMWARE_NV_INDEX);
}

u32 antirollback_lock_space_rec_hash(void)
{
	return tlcl_lock_nv_write(REC_HASH_NV_INDEX);
}

#else

static int _factory_initialise_tpm(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	struct tpm_permanent_flags pflags;
	u32 result;
	int ret;

	result = tpm_get_permanent_flags(&pflags);
	if (result != TPM_SUCCESS)
		return -EIO;

	/*
	 * TPM may come from the factory without physical presence finalized.
	 * Fix if necessary.
	 */
	vboot_log(LOGL_DEBUG, "physical_presence_lifetime_lock=%d\n",
		 pflags.physical_presence_lifetime_lock);
	if (!pflags.physical_presence_lifetime_lock) {
		vboot_log(LOGL_DEBUG, "Finalising physical presence\n");
		result = tpm_finalise_physical_presence();
		if (result != TPM_SUCCESS)
			return -EIO;
	}

	/*
	 * The TPM will not enforce the NV authorization restrictions until the
	 * execution of a TPM_NV_DefineSpace with the handle of
	 * TPM_NV_INDEX_LOCK.  Here we create that space if it doesn't already
	 * exist. */
	vboot_log(LOGL_DEBUG, "nv_locked=%d\n", pflags.nv_locked);
	if (!pflags.nv_locked) {
		vboot_log(LOGL_DEBUG, "Enabling NV locking\n");
		ret = tpm_nv_set_locked();
		if (result != TPM_SUCCESS)
			return -EIO;
	}

	/* Clear TPM owner, in case the TPM is already owned for some reason. */
	vboot_log(LOGL_DEBUG, "TPM: Clearing owner\n");
	result = tpm_clear_and_reenable();
	if (result != TPM_SUCCESS)
		return -EIO;

	/* Define and initialise the kernel space */
	ret = cros_nvdata_setup_walk(CROS_NV_SECDATAK, TPM_NV_PER_PPWRITE,
				     secdata_kernel, sizeof(secdata_kernel));
	if (ret)
		return log_msg_ret("Cannot setup rec_hash_space", ret);

	/* Defines and sets vb2 secdata space */
	vb2api_secdata_create(ctx);
	ret = cros_nvdata_setup_walk(CROS_NV_SECDATA,
				     TPM_NV_PER_GLOBALLOCK | TPM_NV_PER_PPWRITE,
				     ctx->secdata, VB2_SECDATA_SIZE);
	if (ret)
		return log_msg_ret("Cannot setup rec_hash_space", ret);

	/* Define and set rec hash space, if available. */
	if (IS_ENABLED(CONFIG_VBOOT_HAS_REC_HASH_SPACE)) {
		ret = cros_nvdata_setup_walk(CROS_NV_REC_HASH,
			TPM_NV_PER_GLOBALLOCK | TPM_NV_PER_PPWRITE,
			rec_hash_data, REC_HASH_NV_SIZE);
		if (ret)
			return log_msg_ret("Cannot setup rec_hash_space", ret);
	}

	return 0;
}

u32 antirollback_lock_space_firmware(void)
{
	return tpm_set_global_lock();
}

u32 antirollback_lock_space_rec_hash(void)
{
	/*
	 * Nothing needs to be done here, since global lock is already set while
	 * locking firmware space.
	 */
	return TPM_SUCCESS;
}
#endif

/**
 * Perform one-time initializations.
 *
 * Create the NVRAM spaces, and set their initial values as needed.  Sets the
 * nvLocked bit and ensures the physical presence command is enabled and
 * locked.
 */
u32 factory_initialise_tpm(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	u32 result;
	int ret;

	/* Defines and sets vb2 secdata space */
	vb2api_secdata_create(ctx);

	vboot_log(LOGL_DEBUG, "TPM: factory initialization\n");

	/*
	 * Do a full test.  This only happens the first time the device is
	 * turned on in the factory, so performance is not an issue.  This is
	 * almost certainly not necessary, but it gives us more confidence
	 * about some code paths below that are difficult to
	 * test---specifically the ones that set lifetime flags, and are only
	 * executed once per physical TPM.
	 */
	result = tpm_self_test_full();
	if (result != TPM_SUCCESS)
		return -EIO;

	ret = _factory_initialise_tpm(vboot);
	if (ret)
		return ret;

	vboot_log(LOGL_DEBUG, "TPM: factory initialization successful\n");

	return 0;
}

#if IS_ENABLED(CONFIG_TPM_V1)
uint32_t tpm_get_flags(bool *disablep, bool *deactivatedp,
		       bool *nvlockedp)
{
	struct tpm_permanent_flags pflags;
	uint32_t result = tpm_get_permanent_flags(&pflags);

	if (result == TPM_SUCCESS) {
		if (disablep)
			*disablep = pflags.disable;
		if (deactivatedp)
			*deactivatedp = pflags.deactivated;
		if (nvlockedp)
			*nvlockedp = pflags.nv_locked;
		vboot_log(LOGL_DEBUG,
			  "TPM: flags disable=%d, deactivated=%d, nv_locked=%d\n",
			  pflags.disable, pflags.deactivated, pflags.nv_locked);
	}
	return result;
}

static uint32_t tpm1_invoke_state_machine(void)
{
	bool disable;
	bool deactivated;
	uint32_t result = TPM_SUCCESS;

	/* Check that the TPM is enabled and activated. */
	result = tpm_get_flags(&disable, &deactivated, NULL);
	if (result != TPM_SUCCESS) {
		vboot_log(LOGL_ERR, "TPM: Can't read capabilities\n");
		return result;
	}

	if (!!deactivated != IS_ENABLED(CONFIG_TPM_DEACTIVATE)) {
		vboot_log(LOGL_INFO,
			  "TPM: Unexpected TPM deactivated state; toggling..\n");
		result = tpm_physical_set_deactivated(!deactivated);
		if (result != TPM_SUCCESS) {
			vboot_log(LOGL_ERR,
				  "TPM: Can't toggle deactivated state\n");
			return result;
		}

		deactivated = !deactivated;
		result = TPM_E_MUST_REBOOT;
	}

	if (disable && !deactivated) {
		vboot_log(LOGL_INFO, "TPM: disabled (%d). Enabling..\n", disable);

		result = tpm_physical_enable();
		if (result != TPM_SUCCESS) {
			vboot_log(LOGL_ERR, "TPM: Can't set enabled state\n");
			return result;
		}

		vboot_log(LOGL_INFO, "TPM: Must reboot to re-enable\n");
		result = TPM_E_MUST_REBOOT;
	}

	return result;
}
#endif

/*
 * tpm_setup starts the TPM and establishes the root of trust for the
 * anti-rollback mechanism.  SetupTPM can fail for three reasons.  1 A bug. 2 a
 * TPM hardware failure. 3 An unexpected TPM state due to some attack.  In
 * general we cannot easily distinguish the kind of failure, so our strategy is
 * to reboot in recovery mode in all cases.  The recovery mode calls SetupTPM
 * again, which executes (almost) the same sequence of operations.  There is a
 * good chance that, if recovery mode was entered because of a TPM failure, the
 * failure will repeat itself.  (In general this is impossible to guarantee
 * because we have no way of creating the exact TPM initial state at the
 * previous boot.)  In recovery mode, we ignore the failure and continue, thus
 * giving the recovery kernel a chance to fix things (that's why we don't set
 * bGlobalLock).  The choice is between a knowingly insecure device and a
 * bricked device.
 *
 * As a side note, observe that we go through considerable hoops to avoid using
 * the STCLEAR permissions for the index spaces.  We do this to avoid writing
 * to the TPM flashram at every reboot or wake-up, because of concerns about
 * the durability of the NVRAM.
 */
uint32_t tpm_setup(struct vboot_info *vboot, bool s3flag)
{
	uint32_t result;

	result = tpm_open(vboot->tpm);
	if (result != TPM_SUCCESS) {
		vboot_log(LOGL_ERR, "TPM: Can't initialise\n");
		goto out;
	}

	/* Handle special init for S3 resume path */
	if (s3flag) {
		result = tpm_resume();
		if (result == TPM_INVALID_POSTINIT)
			vboot_log(LOGL_INFO, "TPM: Already initialised\n");

		return TPM_SUCCESS;
	}

	result = tpm_startup(TPM_ST_CLEAR);
	if (result != TPM_SUCCESS) {
		vboot_log(LOGL_ERR, "TPM: Can't run startup command\n");
		goto out;
	}

	result = tpm_tsc_physical_presence(TPM_PHYSICAL_PRESENCE_PRESENT);
	if (result != TPM_SUCCESS) {
		/*
		 * It is possible that the TPM was delivered with the physical
		 * presence command disabled.  This tries enabling it, then
		 * tries asserting PP again.
		 */
		result = tpm_tsc_physical_presence(
				TPM_PHYSICAL_PRESENCE_CMD_ENABLE);
		if (result != TPM_SUCCESS) {
			vboot_log(LOGL_ERR,
				  "Can't enable physical presence command\n");
			goto out;
		}

		result = tpm_tsc_physical_presence(
				TPM_PHYSICAL_PRESENCE_PRESENT);
		if (result != TPM_SUCCESS) {
			vboot_log(LOGL_ERR, "Can't assert physical presence\n");
			goto out;
		}
	}

#if IS_ENABLED(CONFIG_TPM_V1)
	result = tpm1_invoke_state_machine();
	if (result != TPM_SUCCESS)
		return result;
#endif

out:
	if (result != TPM_SUCCESS)
		vboot_log(LOGL_ERR, "TPM: setup failed\n");
	else
		vboot_log(LOGL_INFO, "TPM: setup succeeded\n");

	return result;
}

u32 vboot_setup_tpm(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);

	u32 result;

	result = tpm_setup(vboot, ctx->flags & VB2_CONTEXT_S3_RESUME);
	if (result == TPM_E_MUST_REBOOT)
		ctx->flags |= VB2_CONTEXT_SECDATA_WANTS_REBOOT;

	return result;
}

u32 antirollback_read_space_rec_hash(uint8_t *data, u32 size)
{
	if (size != REC_HASH_NV_SIZE) {
		vboot_log(LOGL_DEBUG, "TPM: Incorrect buffer size for rec hash. "
			"(Expected=0x%x Actual=0x%x)\n", REC_HASH_NV_SIZE,
			size);
		return TPM_E_READ_FAILURE;
	}
	return read_space_rec_hash(data);
}

#if 0
u32 antirollback_write_space_rec_hash(const uint8_t *data, u32 size)
{
	uint8_t spc_data[REC_HASH_NV_SIZE];
	u32 rv;

	if (size != REC_HASH_NV_SIZE) {
		vboot_log(LOGL_DEBUG, "TPM: Incorrect buffer size for rec hash. "
			"(Expected=0x%x Actual=0x%x)\n", REC_HASH_NV_SIZE,
			size);
		return TPM_E_WRITE_FAILURE;
	}

	rv = read_space_rec_hash(spc_data);
	if (rv == TPM_BADINDEX) {
		/*
		 * If space is not defined already for recovery hash, define
		 * new space.
		 */
		vboot_log(LOGL_DEBUG, "TPM: Initializing recovery hash space\n");
		return set_rec_hash_space(vboot, data);
	}

	if (rv != TPM_SUCCESS)
		return rv;

	return write_secdata(REC_HASH_NV_INDEX, data, size);
}
#endif

int vb2ex_tpm_clear_owner(struct vb2_context *ctx)
{
	u32 rv;

	vboot_log(LOGL_INFO, "Clearing TPM owner\n");
	rv = tpm_clear_and_reenable();
	if (rv)
		return VB2_ERROR_EX_TPM_CLEAR_OWNER;

	return VB2_SUCCESS;
}
