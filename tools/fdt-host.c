// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google, LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include "mkimage.h"
#include <fdt_region.h>
#include <image.h>
#include <version.h>

/**
 * fdt_setup_sig() - Setup the signing info ready for use
 *
 * @info: Info to set up
 * @keydir: Directory holding private keys
 * @keyfile: Filename of private or public key (NULL to find it in @keydir)
 * @keyname: Name to use for key node if added
 * @blob: FDT blob to sign
 * @algo_name: Algorithm name to use for signing, e.g. "sha256,rsa2048"
 * @padding_name: Padding method to use for signing, NULL if none
 * @require_keys: Mark signing keys as 'required' (else they are optional)
 * @engine_id: Engine to use for signin, NULL for default
 * @return 0 if OK, -ENOMEM if out of memory, -ENOSYS if unsupported algorithm
 */
static int fdt_setup_sig(struct image_sign_info *info, const char *keydir,
			 const char *keyfile, const char *keyname, void *blob,
			 const char *algo_name, const char *padding_name,
			 const char *require_keys, const char *engine_id)
{
	memset(info, '\0', sizeof(*info));
	info->keydir = keydir;
	info->keyfile = keyfile;
	info->keyname = keyname;
	info->fit = blob;
	info->name = strdup(algo_name);
	if (!info->name) {
		printf("Out of memory for algo_name\n");
		return -ENOMEM;
	}
	info->checksum = image_get_checksum_algo(algo_name);
	info->crypto = image_get_crypto_algo(algo_name);
	info->padding = image_get_padding_algo(padding_name);
	info->require_keys = require_keys;
	info->engine_id = engine_id;
	if (!info->checksum || !info->crypto) {
		printf("Unsupported signature algorithm (%s)\n", algo_name);
		return -ENOSYS;
	}

	return 0;
}

/**
 * h_exclude_nodes() - Handles excluding certain nodes from the FDT
 *
 * This is called by fdt_next_region() when it wants to find out if a node or
 * property should be included in the hash.
 *
 * This function simply omits the /chosen node as well as /signature and any
 * subnodes.
 *
 * @return 0 if the node should be excluded, -1 for everything else (meaning it
 *	has no opinion)
 */
static int h_exclude_nodes(void *priv, const void *fdt, int offset, int type,
			   const char *data, int size)
{
	if (type == FDT_IS_NODE) {
		/* Ignore the chosen node as well as /signature and subnodes */
		if (!strcmp("/chosen", data) ||
		    !strncmp("/signature", data, 10))
			return 0;
	}

	return -1;
}

/**
 * run_find_regions() - Use the fdt region calculation to get a list of regions
 *
 * This finds the regions of the FDT that need to be hashed so that it can be
 * protected against modification by a signature
 *
 * @fdt: FDT blob to check
 * @include_func: Function to call to determine whether to include an element of
 * the devicetree
 * @priv: Private data, set to NULL for now
 * @region: Region list to fill in
 * @max_regions: Max size of region list, e.g. 100
 * @path: Place to keep the path to each node (used by this function)
 * @path_len: Max size of @path, 200 bytes is recommended
 * @return 0 if OK, -FDT_ERR_... on error
 */
static int run_find_regions(const void *fdt,
			    int (*include_func)(void *priv, const void *fdt,
						int offset, int type,
						const char *data, int size),
			    void *priv, struct fdt_region *region,
			    int max_regions, char *path, int path_len,
			    int flags)
{
	struct fdt_region_state state;
	int count;
	int ret;

	count = 0;
	ret = fdt_first_region(fdt, include_func, NULL, &region[count++], path,
			       path_len, flags, &state);
	while (ret == 0) {
		ret = fdt_next_region(fdt, include_func, NULL,
				      count < max_regions ? &region[count] :
				      NULL, path, path_len, flags, &state);
		if (!ret)
			count++;
	}
	if (ret && ret != -FDT_ERR_NOTFOUND)
		return ret;

	return count;
}

int fdt_get_regions(const void *blob, int strtab_len,
		    struct image_region **regionp, int *region_countp)
{
	struct image_region *region;
	struct fdt_region fdt_regions[100];
	char path[200];
	int count;

	/* Get a list of regions to hash */
	count = run_find_regions(blob, h_exclude_nodes, NULL, fdt_regions,
				 ARRAY_SIZE(fdt_regions), path, sizeof(path),
				 FDT_REG_SUPERNODES | FDT_REG_ADD_MEM_RSVMAP |
				 FDT_REG_ADD_STRING_TAB);
	if (count < 0) {
		fprintf(stderr, "Failed to hash device tree\n");
		return -EIO;
	}
	if (count == 0) {
		fprintf(stderr, "No data to hash for device tree");
		return -EINVAL;
	}

	/* Limit the string table to what was hashed */
	if (strtab_len != -1) {
		if (strtab_len < 0 ||
		    strtab_len > fdt_regions[count - 1].size) {
			fprintf(stderr, "Invalid string-table offset\n");
			return -EINVAL;
		}
		fdt_regions[count - 1].size = strtab_len;
	}

	/* Build our list of data blocks */
	region = fit_region_make_list(blob, fdt_regions, count, NULL);
	if (!region) {
		fprintf(stderr, "Out of memory making region list'\n");
		return -ENOMEM;
	}
#ifdef DEBUG
	int i;

	printf("Regions:\n");
	for (i = 0; i < count; i++) {
		printf("region %d: %x %x %x\n", i, fdt_regions[i].offset,
		       fdt_regions[i].size,
		       fdt_regions[i].offset + fdt_regions[i].size);
	}
#endif
	*region_countp = count;
	*regionp = region;

	return 0;
}

/**
 * fdt_write_sig() - write the signature to an FDT
 *
 * This writes the signature and signer data to the FDT
 *
 * @blob: pointer to the FDT header
 * @noffset: hash node offset
 * @value: signature value to be set
 * @value_len: signature value length
 * @comment: Text comment to write (NULL for none)
 *
 * returns
 *     offset of node where things were added, on success
 *     -FDT_ERR_..., on failure
 */
static int fdt_write_sig(void *blob, uint8_t *value, int value_len,
			 const char *algo_name, const char *sig_name,
			 const char *comment, const char *cmdname)
{
	uint32_t strdata[2];
	int string_size;
	int sigs_node, noffset;
	int ret;

	/*
	 * Get the current string size, before we update the FIT and add
	 * more
	 */
	string_size = fdt_size_dt_strings(blob);

	sigs_node = fdt_subnode_offset(blob, 0, FIT_SIG_NODENAME);
	if (sigs_node == -FDT_ERR_NOTFOUND)
		sigs_node = fdt_add_subnode(blob, 0, FIT_SIG_NODENAME);
	if (sigs_node < 0)
		return sigs_node;

	/* Create a node for this signature */
	noffset = fdt_subnode_offset(blob, sigs_node, sig_name);
	if (noffset == -FDT_ERR_NOTFOUND)
		noffset = fdt_add_subnode(blob, sigs_node, sig_name);
	if (noffset < 0)
		return noffset;

	ret = fdt_setprop(blob, noffset, FIT_VALUE_PROP, value, value_len);
	if (!ret) {
		ret = fdt_setprop_string(blob, noffset, FIT_ALGO_PROP,
					 algo_name);
	}
	if (!ret) {
		ret = fdt_setprop_string(blob, noffset, "signer-name",
					 "fdt_sign");
	}
	if (!ret) {
		ret = fdt_setprop_string(blob, noffset, "signer-version",
					 PLAIN_VERSION);
	}
	if (comment && !ret)
		ret = fdt_setprop_string(blob, noffset, "comment", comment);
	if (!ret) {
		time_t timestamp = imagetool_get_source_date(cmdname,
							     time(NULL));
		uint32_t t = cpu_to_uimage(timestamp);

		ret = fdt_setprop(blob, noffset, FIT_TIMESTAMP_PROP, &t,
				  sizeof(uint32_t));
	}

	/* This is a legacy offset, it is unused, and must remain 0. */
	strdata[0] = 0;
	strdata[1] = cpu_to_fdt32(string_size);
	if (!ret) {
		ret = fdt_setprop(blob, noffset, "hashed-strings",
				  strdata, sizeof(strdata));
	}
	if (ret)
		return ret;

	return noffset;
}

static int fdt_process_sig(const char *keydir, const char *keyfile,
			   void *keydest, void *blob, const char *keyname,
			   const char *comment, int require_keys,
			   const char *engine_id, const char *cmdname,
			   struct image_summary *summary)
{
	struct image_sign_info info;
	struct image_region *region;
	int region_count;
	uint8_t *value;
	uint value_len;
	int ret;

	ret = fdt_get_regions(blob, -1, &region, &region_count);
	if (ret)
		return ret;
	ret = fdt_setup_sig(&info, keydir, keyfile, keyname, blob,
			    "sha256,rsa2048", NULL, require_keys ? "fdt" : NULL,
			    engine_id);
	if (ret)
		return ret;

	ret = info.crypto->sign(&info, region, region_count, &value,
				&value_len);
	free(region);
	if (ret) {
		fprintf(stderr, "Failed to sign FDT\n");

		/* We allow keys to be missing */
		if (ret == -ENOENT)
			return 0;
		return -1;
	}

	ret = fdt_write_sig(blob, value, value_len, info.name, keyname, comment,
			    cmdname);
	if (ret < 0) {
		if (ret == -FDT_ERR_NOSPACE)
			return -ENOSPC;
		printf("Can't write signature: %s\n", fdt_strerror(ret));
		return -1;
	}
	summary->sig_offset = ret;
	fdt_get_path(blob, ret, summary->sig_path,
		     sizeof(summary->sig_path));
	free(value);

	/* Write the public key into the supplied FDT file */
	if (keydest) {
		ret = info.crypto->add_verify_data(&info, keydest);
		if (ret < 0) {
			if (ret != -ENOSPC)
				fprintf(stderr, "Failed to add verification data (err=%d)\n",
					ret);
			return ret;
		}
		summary->keydest_offset = ret;
		fdt_get_path(keydest, ret, summary->keydest_path,
			     sizeof(summary->keydest_path));
	}

	return 0;
}

/* This function exists just to mirror fit_image_add_verification_data() */
int fdt_add_verif_data(const char *keydir, const char *keyfile, void *keydest,
		       void *blob, const char *keyname, const char *comment,
		       bool require_keys, const char *engine_id,
		       const char *cmdname, struct image_summary *summary)
{
	int ret;

	ret = fdt_process_sig(keydir, keyfile, keydest, blob, keyname, comment,
			      require_keys, engine_id, cmdname, summary);

	return ret;
}

#ifdef CONFIG_FIT_SIGNATURE
int fdt_check_sign(const void *blob, const void *key)
{
	int fdt_sigs;
	int ret;

	fdt_sigs = fdt_subnode_offset(blob, 0, FIT_SIG_NODENAME);
	if (fdt_sigs < 0) {
		printf("No %s node found (err=%d)\n", FIT_SIG_NODENAME,
		       fdt_sigs);
		return fdt_sigs;
	}

	ret = fdt_sig_verify(blob, fdt_sigs, key);
	fprintf(stderr, "Verify %s\n", ret ? "failed" : "OK");

	return ret;
}
#endif
