// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2018 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <tpm-v1.h>
#include <cros/antirollback.h>
#include <cros/nvdata.h>

int get_location(enum cros_nvdata_index index)
{
	switch (index) {
	case CROS_NV_DATA:
		/* We cannot handle this */
		break;
	case CROS_NV_SECDATA:
		return FIRMWARE_NV_INDEX;
	case CROS_NV_SECDATAK:
		return KERNEL_NV_INDEX;
	case CROS_NV_REC_HASH:
		return REC_HASH_NV_INDEX;
	}
	log(UCLASS_CROS_NVDATA, LOGL_ERR, "Unsupported index %x\n", index);

	return -1;
}

/**
 * Like tlcl_write(), but checks for write errors due to hitting the 64-write
 * limit and clears the TPM when that happens.  This can only happen when the
 * TPM is unowned, so it is OK to clear it (and we really have no choice).
 * This is not expected to happen frequently, but it could happen.
 */

static u32 safe_write(u32 index, const void *data, u32 length)
{
	u32 ret = tpm_nv_write_value(index, data, length);

	if (ret == TPM_MAXNVWRITES) {
		ret = tpm_clear_and_reenable();
		if (ret != TPM_SUCCESS) {
			log(UCLASS_TPM, LOGL_ERR,
			    "Unable to clear and re-enable TPM\n");
			return ret;
		}
		return tpm_nv_write_value(index, data, length);
	} else {
		return ret;
	}
}

int tpm_secdata_read(struct udevice *dev, uint index, uint8_t *data, int size)
{
	int location;
	int ret;

	location = get_location(index);
	if (location == -1)
		return -ENOSYS;

	ret = tpm_nv_read_value(location, data, size);
	if (ret == TPM_BADINDEX) {
		return log_msg_ret("TPM has no secdata for location", -ENOENT);
	} else if (ret != TPM_SUCCESS) {
		log(UCLASS_CROS_NVDATA, LOGL_ERR,
		    "Failed to read secdata (err=%x)\n", ret);
		return -EIO;
	}

	return 0;
}

static int tpm_secdata_write(struct udevice *dev, uint index,
			     const uint8_t *data, int size)
{
	int location;
	int ret;

	location = get_location(index);
	if (location == -1)
		return -ENOSYS;

	ret = safe_write(location, data, size);
	if (ret != TPM_SUCCESS) {
		log(UCLASS_CROS_NVDATA, LOGL_ERR,
		    "Failed to write secdata (err=%x)\n", ret);
		return -EIO;
	}

	return 0;
}

/**
 * Similarly to safe_write(), this ensures we don't fail a DefineSpace because
 * we hit the TPM write limit. This is even less likely to happen than with
 * writes because we only define spaces once at initialization, but we'd
 * rather be paranoid about this.
 */
static u32 safe_define_space(u32 index, u32 perm, u32 size)
{
	u32 result;

	result = tpm_nv_define_space(index, perm, size);
	if (result == TPM_MAXNVWRITES) {
		result = tpm_clear_and_reenable();
		if (result != TPM_SUCCESS)
			return result;
		return tpm_nv_define_space(index, perm, size);
	} else {
		return result;
	}
}


static int tpm_secdata_setup(struct udevice *dev, uint index, uint attr,
			     const uint8_t *data, int size)
{
	int ret;

	ret = safe_define_space(FIRMWARE_NV_INDEX, attr, size);
	if (ret != TPM_SUCCESS) {
		log(UCLASS_CROS_NVDATA, LOGL_ERR,
		    "Failed to setup secdata (err=%x)\n", ret);
		return -EIO;
	}

	return tpm_secdata_write(dev, index, data, size);
}


static const struct cros_nvdata_ops tpm_secdata_ops = {
	.read	= tpm_secdata_read,
	.write	= tpm_secdata_write,
	.setup	= tpm_secdata_setup,
};

static const struct udevice_id tpm_secdata_ids[] = {
	{ .compatible = "google,tpm-secdata" },
	{ }
};

U_BOOT_DRIVER(tpm_secdata_drv) = {
	.name		= "cros-ec-secdata",
	.id		= UCLASS_CROS_NVDATA,
	.of_match	= tpm_secdata_ids,
	.ops		= &tpm_secdata_ops,
};
