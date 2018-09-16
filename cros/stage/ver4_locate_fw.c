/*
 * Copyright (c) 2017 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <misc.h>
#include <os.h>
#include <spi_flash.h>
#include <cros/fwstore.h>
#include <cros/vboot.h>

/* The max hash size to expect is for SHA512. */
#define VBOOT_MAX_HASH_SIZE	VB2_SHA512_DIGEST_SIZE

/* Buffer size for reading from firmware */
#define TODO_BLOCK_SIZE 1024

static int handle_digest_result(void *slot_hash, size_t slot_hash_sz)
{
	int ret, is_resume;

	/*
	 * Chrome EC is the only support for vboot_save_hash() &
	 * vboot_retrieve_hash(), if Chrome EC is not enabled then return.
	 */
	if (!IS_ENABLED(CONFIG_EC_GOOGLE_CHROMEEC))
		return 0;

	/*
	 * Nothing to do since resuming on the platform doesn't require
	 * vboot verification again.
	 */
	if (!IS_ENABLED(CONFIG_RESUME_PATH_SAME_AS_BOOT))
		return 0;

	/*
	 * Assume that if vboot doesn't start in bootblock verified
	 * RW memory init code is not employed. i.e. memory init code
	 * lives in RO CBFS.
	 */
	if (!IS_ENABLED(CONFIG_VBOOT_STARTS_IN_BOOTBLOCK))
		return 0;

	is_resume = vboot_platform_is_resuming();
	if (is_resume > 0) {
		uint8_t saved_hash[VBOOT_MAX_HASH_SIZE];
		const size_t saved_hash_sz = sizeof(saved_hash);

		assert(slot_hash_sz == saved_hash_sz);

		vboot_log(LOGL_DEBUG, "Platform is resuming\n", ret);

		ret = vboot_retrieve_hash(saved_hash, saved_hash_sz);
		if (ret) {
			vboot_log(LOGL_ERR, "Couldn't retrieve saved hash\n");
			return ret;
		}

		if (memcmp(saved_hash, slot_hash, slot_hash_sz)) {
			vboot_log(LOGL_ERR, "Hash mismatch on resume\n");
			return ret;
		}
	} else if (is_resume < 0) {
		vboot_log(LOGL_ERR,
			  "Unable to determine if platform resuming (%d)",
			  is_resume);
	}

	vboot_log(LOGL_DEBUG, "Saving vboot hash\n");

	/* Always save the hash for the current boot. */
	if (vboot_save_hash(slot_hash, slot_hash_sz)) {
		vboot_log(LOGL_ERR, "Error saving vboot hash\n");
		/*
		 * Though this is an error, don't report it up since it could
		 * lead to a reboot loop. The consequence of this is that
		 * we will most likely fail resuming because of EC issues or
		 * the hash digest not matching.
		 */
		return 0;
	}

	return 0;
}

static int hash_body(struct vboot_info *vboot, struct udevice *fw_main)
{
	const size_t hash_digest_sz = VBOOT_MAX_HASH_SIZE;
	uint8_t hash_digest[VBOOT_MAX_HASH_SIZE];
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	uint8_t block[TODO_BLOCK_SIZE];
	uint32_t expected_size;
	int ret;

	vboot_log(LOGL_ERR, "Hashing firmware body\n");
	/*
	 * Clear the full digest so that any hash digests less than the
	 * max size have trailing zeros
	 */
	memset(hash_digest, 0, hash_digest_sz);

	bootstage_mark(BOOTSTAGE_VBOOT_START_HASH_BODY);

	expected_size = fwstore_reader_size(fw_main);

	/* Start the body hash */
	ret = vb2api_init_hash(ctx, VB2_HASH_TAG_FW_BODY, &expected_size);
	if (ret)
		return log_msg_ret("init hash", ret);

	/*
	 * Honor vboot's RW slot size. The expected size is pulled out of
	 * the preamble and obtained through vb2api_init_hash() above. By
	 * creating sub region the RW slot portion of the boot media is
	 * limited.
	 */
	ret = fwstore_reader_restrict(fw_main, 0, expected_size);
	if (ret) {
		vboot_log(LOGL_ERR, "Unable to restrict firmware size\n");
		return ret;
	}

	/* Extend over the body */
	while (1) {
		int nbytes;

		bootstage_start(BOOTSTAGE_ACCUM_VBOOT_FIRMWARE_READ, NULL);
		nbytes = misc_read(fw_main, -1, block, TODO_BLOCK_SIZE);
		bootstage_accum(BOOTSTAGE_ACCUM_VBOOT_FIRMWARE_READ);
		if (nbytes < 0)
			return log_msg_ret("Read fwstore", nbytes);
		else if (!nbytes)
			break;

		ret = vb2api_extend_hash(ctx, block, nbytes);
		if (ret)
			return log_msg_ret("extend hash", ret);
	}
	bootstage_mark(BOOTSTAGE_VBOOT_DONE_HASHING);

	/* Check the result (with RSA signature verification) */
	ret = vb2api_check_hash_get_digest(ctx, hash_digest, hash_digest_sz);
	if (ret)
		return log_msg_ret("check hash", ret);

	bootstage_mark(BOOTSTAGE_VBOOT_END_HASH_BODY);

	if (handle_digest_result(hash_digest, hash_digest_sz))
		return log_msg_ret("handle result", ret);
	vboot->fw_size = expected_size;

	return VB2_SUCCESS;
}

int vboot_ver4_locate_fw(struct vboot_info *vboot)
{
	struct fmap_entry *entry;
	struct udevice *dev;
	int ret;

	if (vboot_is_slot_a(vboot))
		entry = &vboot->fmap.readwrite_a.spl;
	else
		entry = &vboot->fmap.readwrite_b.spl;
	ret = fwstore_get_reader_dev(vboot->fwstore, entry->offset,
				     entry->length, &dev);
	/* TODO(sjg@chromium.org): Perhaps this should be fatal? */
	if (ret)
		return log_msg_ret("Cannot get reader device", ret);

	ret = hash_body(vboot, dev);
	if (ret) {
		vboot_log(LOGL_INFO, "Reboot requested (%x)\n", ret);
		return VBERROR_REBOOT_REQUIRED;
	}

	return 0;
}
