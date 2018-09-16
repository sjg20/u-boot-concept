// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 Google, Inc
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <bloblist.h>
#include <dm.h>
#include <log.h>
#include <cros/antirollback.h>
#include <cros/vboot.h>
#include <cros/nvdata.h>

/**
 * vb2_init_blob() - Set up the vboot persistent blob
 *
 * This holds struct vboot_blob which includes things we need to know and also
 * the vb2_context. Set up a work buffer within the vb2_context.
 *
 * The blob is called persistent since it is preserved through each stage of
 * the boot.
 *
 * @blob: Pointer to the persistent blob for vboot
 */
static int vb2_init_blob(struct vboot_blob *blob)
{
	struct vb2_context *ctx = &blob->ctx;

	/* Initialize the vb2_context. */
	memset(blob, '\0', sizeof(*blob));
	ctx->workbuf_size = CONFIG_VBOOT2_WORK_BUF_SIZE;
	ctx->workbuf = memalign(VBOOT_CONTEXT_ALIGN, ctx->workbuf_size);
	if (!ctx->workbuf)
		return -ENOMEM;
	memset(ctx->workbuf, '\0', ctx->workbuf_size);

	return 0;
}

int vboot_ver_init(struct vboot_info *vboot)
{
	struct vboot_blob *blob;
	struct vb2_context *ctx;
	int ret;

	printf("vboot is at %p, size %lx\n", vboot, (ulong)sizeof(*vboot));
	blob = bloblist_add(BLOBLISTT_VBOOT_CTX, sizeof(struct vboot_blob));
	if (!blob)
		return log_msg_ret("Cannot set up vboot context", -ENOSPC);

	bootstage_mark(BOOTSTAGE_VBOOT_START);

	/* Set up context and work buffer */
	ret = vb2_init_blob(blob);
	if (ret)
		return log_msg_ret("Cannot set up work context", ret);
	vboot->blob = blob;
	ctx = &blob->ctx;
	vboot->ctx = ctx;
	ctx->non_vboot_context = vboot;
	vboot->valid = true;
	printf("ctx=%p\n", ctx);

	vboot->config = cros_ofnode_config_node();

	ret = uclass_first_device_err(UCLASS_TPM, &vboot->tpm);
	if (ret)
		return log_msg_ret("Cannot find TPM", ret);
	ret = vboot_setup_tpm(vboot);
	if (ret) {
		vboot_log(LOGL_ERR, "TPM setup failed (err=%x)\n", ret);
		return -EIO;
	}

	/* Initialize and read nvdata from non-volatile storage */
	ret = uclass_first_device_err(UCLASS_CROS_NVDATA, &vboot->nvdata_dev);
	if (ret)
		return log_msg_ret("Cannot find nvdata", ret);
	ret = cros_nvdata_read_walk(CROS_NV_DATA, ctx->nvdata,
				    sizeof(ctx->nvdata));
	if (ret)
		return log_msg_ret("Cannot read nvdata", ret);
	print_buffer(0, ctx->nvdata, 1, sizeof (ctx->nvdata), 0);

	ret = cros_ofnode_flashmap(&vboot->fmap);
	if (ret)
		return log_msg_ret("failed to decode fmap\n", ret);
	dump_fmap(&vboot->fmap);
	ret = uclass_first_device_err(UCLASS_CROS_FWSTORE, &vboot->fwstore);
	if (ret)
		return log_msg_ret("Cannot set up fwstore", ret);

	if (CONFIG_IS_ENABLED(CROS_EC)) {
		ret = uclass_get_device(UCLASS_CROS_EC, 0, &vboot->cros_ec);
		if (ret)
			return log_msg_ret("Cannot locate Chromium OS EC", ret);
	}
	/*
	 * Set S3 resume flag if vboot should behave differently when selecting
	 * which slot to boot.  This is only relevant to vboot if the platform
	 * does verification of memory init and thus must ensure it resumes with
	 * the same slot that it booted from.
	 * */
	if (IS_ENABLED(CONFIG_RESUME_PATH_SAME_AS_BOOT) &&
	    IS_ENABLED(CONFIG_VBOOT_STARTS_IN_BOOTBLOCK) &&
	    vboot_platform_is_resuming())
		ctx->flags |= VB2_CONTEXT_S3_RESUME;

	/*
	 * Read secdata from TPM. Initialize TPM if secdata not found. We don't
	 * check the return value here because vb2api_fw_phase1 will catch
	 * invalid secdata and tell us what to do (=reboot).
	 */
	bootstage_mark(BOOTSTAGE_VBOOT_START_TPMINIT);
	ret = cros_nvdata_read_walk(CROS_NV_SECDATA, ctx->secdata,
				    sizeof(ctx->secdata));
	if (ret == -ENOENT)
		ret = factory_initialise_tpm(vboot);
	else if (ret)
		return log_msg_ret("Cannot read secdata", ret);

	bootstage_mark(BOOTSTAGE_VBOOT_END_TPMINIT);

	if (vboot_flag_read_walk(VBOOT_FLAG_DEVELOPER) == 1)
		ctx->flags |= VB2_CONTEXT_FORCE_DEVELOPER_MODE;

	if (vboot_flag_read_walk(VBOOT_FLAG_RECOVERY) == 1) {
		if (IS_ENABLED(CONFIG_VBOOT_DISABLE_DEV_ON_RECOVERY))
			ctx->flags |= VB2_DISABLE_DEVELOPER_MODE;
	}

	if (vboot_flag_read_walk(VBOOT_FLAG_WIPEOUT) == 1)
		ctx->flags |= VB2_CONTEXT_FORCE_WIPEOUT_MODE;

	if (vboot_flag_read_walk(VBOOT_FLAG_LID_OPEN) == 0)
		ctx->flags |= VB2_CONTEXT_NOFAIL_BOOT;

	return 0;
}
