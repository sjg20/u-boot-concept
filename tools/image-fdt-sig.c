// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google, LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include "mkimage.h"
#include <image.h>
#include <u-boot/rsa.h>

/**
 * fdt_setup_sig() - Setup the signing info ready for use
 *
 * @info: Info to set up
 * @blob: FDT blob to check
 * @noffset: Offset of signature to verify, e.g. '/signatures/sig-1'
 * @key_blob: FDT blob containing keys to verify against
 * @required_keynode: Node containing the key to verify
 * @err_msgp: Returns the error messages if something goes wrong
 * @return 0 if OK, -E2BIG if the FDT is too large, -ENOSYS if unsupported
 *	algorithm, -EINVAL if the algorithm name is not present in the signature
 */
static int fdt_image_setup_verify(struct image_sign_info *info,
				  const void *blob, int noffset,
				  const void *key_blob, int required_keynode,
				  char **err_msgp)
{
	char *algo_name;
	const char *padding_name;

	if (fdt_totalsize(blob) > CONFIG_VAL(FIT_SIGNATURE_MAX_SIZE)) {
		*err_msgp = "Total size too large";
		return -E2BIG;
	}
	if (fit_image_hash_get_algo(blob, noffset, &algo_name)) {
		*err_msgp = "Can't get hash algo property";
		return -EINVAL;
	}

	padding_name = fdt_getprop(blob, noffset, "padding", NULL);
	if (!padding_name)
		padding_name = RSA_DEFAULT_PADDING_NAME;

	memset(info, '\0', sizeof(*info));
	info->keyname = fdt_getprop(blob, noffset, FIT_KEY_HINT, NULL);
	info->fit = blob;
	info->node_offset = noffset;
	info->name = algo_name;
	info->checksum = image_get_checksum_algo(algo_name);
	info->crypto = image_get_crypto_algo(algo_name);
	info->padding = image_get_padding_algo(padding_name);
	info->fdt_blob = key_blob;
	info->required_keynode = required_keynode;

	if (!info->checksum || !info->crypto || !info->padding) {
		*err_msgp = "Unknown signature algorithm";
		return -ENOSYS;
	}

	return 0;
}

/**
 * fdt_verify_sig() - Verify that a signature can be verified with a key
 *
 * This checks a particular key to see if it verifies the given signature
 *
 * @blob: FDT blob to verify
 * @noffset: Offset of signature to verify, e.g. '/signatures/sig-1'
 * @key_blob: FDT blob containing keys to verify against
 * @required_keynode: Node containing the key to verify
 * @err_msgp: Returns the error messages if something goes wrong
 * @return 0 if OK, -EPERM if the key could not be verified
 */
static int fdt_check_sig(const void *blob, int noffset, const void *key_blob,
			 int required_keynode, char **err_msgp)
{
	int region_count, strtab_len;
	struct image_sign_info info;
	struct image_region *region;
	const uint32_t *strings;
	uint8_t *fdt_value;
	int fdt_value_len;
	int size;
	int ret;

	if (fdt_image_setup_verify(&info, blob, noffset, key_blob,
				   required_keynode, err_msgp))
		return -EPERM;

	if (fit_image_hash_get_value(blob, noffset, &fdt_value,
				     &fdt_value_len)) {
		*err_msgp = "Can't get hash value property";
		return -EPERM;
	}

	/* Add the strings */
	strings = fdt_getprop(blob, noffset, "hashed-strings", &size);
	if (!strings) {
		*err_msgp = "Missing 'hashed-strings' property";
		return -EINVAL;
	}
	if (size != sizeof(u32) * 2) {
		*err_msgp = "Invalid 'hashed-strings' property";
		return -EINVAL;
	}
	strtab_len = fdt32_to_cpu(strings[1]);
	debug("%s: strtab_len=%x\n", __func__, strtab_len);

	ret = fdt_get_regions(blob, strtab_len, &region, &region_count);
	if (ret) {
		*err_msgp = "Cannot get regions";
		return ret;
	}

	ret = info.crypto->verify(&info, region, region_count, fdt_value,
				  fdt_value_len);
	free(region);
	if (ret) {
		*err_msgp = "Verification failed";
		return -EPERM;
	}

	return 0;
}

/**
 * fdt_verify_sig() - Verify that a signature exists for a key
 *
 * This checks a particular key to see if it verifies. It tries all signatures
 * to find a match.
 *
 * @blob: FDT blob to verify
 * @fdt_sigs: Offset of /signatures node in blob
 * @key_blob: FDT blob containing keys to verify against
 * @required_keynode: Node containing the key to verify
 * @return 0 if OK, -EPERM if the key could not be verified
 */
static int fdt_verify_sig(const void *blob, int fdt_sigs,
			  const void *key_blob, int required_keynode)
{
	char *err_msg = "No 'signature' subnode found";
	int bad_noffset = -1;
	int noffset;
	int verified = 0;
	int ret;

	/* Process all subnodes of the /signature node */
	fdt_for_each_subnode(noffset, blob, fdt_sigs) {
		printf("%s", fdt_get_name(blob, noffset, NULL));
		ret = fdt_check_sig(blob, noffset, key_blob, required_keynode,
				    &err_msg);
		if (ret) {
			printf("- ");
			bad_noffset = noffset;
		} else {
			printf("+ ");
			verified = 1;
			break;
		}
	}

	if (noffset == -FDT_ERR_TRUNCATED || noffset == -FDT_ERR_BADSTRUCTURE) {
		err_msg = "Corrupted or truncated tree";
		goto error;
	}

	if (verified) {
		printf("\n");
		return 0;
	}

error:
	printf(" error!\n%s for node '%s'\n", err_msg,
	       fdt_get_name(blob, bad_noffset, NULL));
	return -EPERM;
}

/**
 * fdt_verify_required_sigs() - Verify that required signatures are valid
 *
 * This works through all the provided keys, checking for a signature that uses
 * that key. If the key is required, then it must verify correctly. If it is not
 * required, then the failure is ignored, just displayed for informational
 * purposes.
 *
 * If the "required-mode" property is present and set to "any" then only one of
 * the required keys needs to be verified
 *
 * @blob: FDT blob to verify
 * @fdt_sigs: Offset of /signatures node in blob
 * @key_blob: FDT blob containing keys to verify against
 * @return 0 if OK, -EPERM if required keys could not be verified (either any or
 *	all), -EINVAL if the FDT is missing something
 */
static int fdt_verify_required_sigs(const void *blob, int fdt_sigs,
				    const void *key_blob)
{
	int key_node;
	int keys_node;
	int verified = 0;
	int reqd_sigs = 0;
	bool reqd_policy_all = true;
	const char *reqd_mode;

	/* Work out what we need to verify */
	keys_node = fdt_subnode_offset(key_blob, 0, FIT_SIG_NODENAME);
	if (keys_node < 0) {
		debug("%s: No signature node found: %s\n", __func__,
		      fdt_strerror(keys_node));
		return 0;
	}

	/* Get required-mode policy property from DTB */
	reqd_mode = fdt_getprop(key_blob, keys_node, "required-mode", NULL);
	if (reqd_mode && !strcmp(reqd_mode, "any"))
		reqd_policy_all = false;

	debug("%s: required-mode policy set to '%s'\n", __func__,
	      reqd_policy_all ? "all" : "any");

	/* Check each key node */
	fdt_for_each_subnode(key_node, key_blob, keys_node) {
		const char *required_prop;
		bool required;
		int ret;

		required_prop = fdt_getprop(key_blob, key_node, FIT_KEY_REQUIRED,
					    NULL);
		required = required_prop && !strcmp(required_prop, "fdt");

		if (required)
			reqd_sigs++;

		ret = fdt_verify_sig(blob, fdt_sigs, key_blob, key_node);
		if (!ret && required)
			verified++;
	}

	if (reqd_policy_all && reqd_sigs != verified) {
		fprintf(stderr, "Failed to verify all required signature\n");
		return -EPERM;
	} else if (reqd_sigs && !verified) {
		fprintf(stderr, "Failed to verify 'any' of the required signature(s)\n");
		return -EPERM;
	}

	return 0;
}

int fdt_sig_verify(const void *blob, int fdt_sigs, const void *key)
{
	return fdt_verify_required_sigs(blob, fdt_sigs, key);
}
