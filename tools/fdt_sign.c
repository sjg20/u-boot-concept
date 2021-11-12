// SPDX-License-Identifier: GPL-2.0+
/*
 * Sign an FDT file
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include "mkimage.h"
#include "fit_common.h"
#include <image.h>

void usage(char *cmdname)
{
	fprintf(stderr,
		"Usage: %s -f dtb_file -G file.key -k dir [-K pub.dtb] [-o out_file]\n"
		"          -f ==> set dtb file which should be signed\n"
		"          -G ==> set signing key to use\n"
		"          -k ==> set directory containing private keys\n"
		"          -K ==> set DTB file to receive signing key\n"
		"          -o ==> if not provided, dtb file is updated\n"
		"          -S ==> name to use for signaure (defaults to -G)\n"
		"          -r ==> mark keys as required to be verified\n"
		"          -q ==> quiet mode\n",
		cmdname);
	exit(EXIT_FAILURE);
}

/**
 * sign_fdt() - Sign an FDT
 *
 * @params: Image parameters
 * @destfile: Filename of FDT being signed (only used for messages)
 * @size_inc: Amount to expand the FDT blob by, before adding signing data
 * @blob: FDT blob to sign
 * @size: Size of blob in bytes
 * @return 0 if OK, -ve on error
 */
static int sign_fdt(struct image_tool_params *params, const char *destfile,
		    size_t size_inc, char *blob, int size)
{
	struct image_summary summary;
	void *dest_blob = NULL;
	struct stat dest_sbuf;
	char def_name[80];
	int destfd = 0;
	int ret;

	/*
	 * If we don't have a signature name, try to make one from the keyfile.
	 * '/path/to/dir/name.key' becomes 'name'
	 */
	if (!params->keyname) {
		const char *p = strrchr(params->keyfile, '/');
		char *q;

		if (p)
			p++;
		else
			p = params->keyfile;
		strncpy(def_name, p, sizeof(def_name));
		def_name[sizeof(def_name) - 1] = '\0';
		q = strstr(def_name, ".key");
		if (q && q[strlen(q)] == '\0')
			*q = '\0';
		params->keyname = def_name;
	}

	if (params->keydest) {
		destfd = mmap_fdt(params->cmdname, params->keydest, size_inc,
				  &dest_blob, &dest_sbuf, false,
				  false);
		if (destfd < 0) {
			ret = -EIO;
			fprintf(stderr, "Cannot open keydest file '%s'\n",
				params->keydest);
		}
	}

	ret = fdt_add_verif_data(params->keydir, params->keyfile, dest_blob,
				 blob, params->keyname, params->comment,
				 params->require_keys, params->engine_id,
				 params->cmdname, &summary);
	if (!ret && !params->quiet)
		summary_show(&summary, destfile, params->keydest);

	if (params->keydest) {
		(void)munmap(dest_blob, dest_sbuf.st_size);
		close(destfd);
	}
	if (ret < 0) {
		if (ret != -ENOSPC)
			fprintf(stderr, "Failed to add signature\n");
		return ret;
	}

	return ret;
}

/**
 * do_fdt_sign() - Sign an FDT, expanding if needed
 *
 * If a separate output file is specified, the FDT blob is copied to that first.
 *
 * If there is not space in the FDT to add the signature, it is expanded
 * slightly and the operation is retried.
 *
 * @params: Image parameters
 * @cmdname: Name of tool (e.g. argv[0]), to use in error messages
 * @fdtfile: Filename of FDT blob to sign
 * @return 0 if OK, -ve on error
 */
static int do_fdt_sign(struct image_tool_params *params, const char *cmdname,
		       const char *fdtfile)
{
	const char *destfile;
	struct stat fsbuf;
	int ffd, size_inc;
	void *blob;
	int ret;

	destfile = params->outfile ? params->outfile : fdtfile;
	for (size_inc = 0; size_inc < 64 * 1024;) {
		if (params->outfile) {
			if (copyfile(fdtfile, params->outfile) < 0) {
				printf("Can't copy %s to %s\n", fdtfile,
				       params->outfile);
				return -EIO;
			}
		}
		ffd = mmap_fdt(cmdname, destfile, size_inc, &blob, &fsbuf,
			       params->outfile, false);
		if (ffd < 0)
			return -1;

		ret = sign_fdt(params, destfile, 0, blob, fsbuf.st_size);
		(void)munmap((void *)blob, fsbuf.st_size);
		close(ffd);

		if (ret >= 0 || ret != -ENOSPC)
			break;
		size_inc += 512;
		debug("Not enough space in FDT '%s', trying size_inc=%#x\n",
		      destfile, size_inc);
	}

	if (ret < 0) {
		fprintf(stderr, "Failed to sign '%s' (error %d)\n",
			destfile, ret);
		return ret;
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct image_tool_params params;
	char *fdtfile = NULL;
	char cmdname[256];
	int ret;
	int c;

	memset(&params, '\0', sizeof(params));
	strncpy(cmdname, *argv, sizeof(cmdname) - 1);
	cmdname[sizeof(cmdname) - 1] = '\0';
	while ((c = getopt(argc, argv, "f:G:k:K:o:qrS:")) != -1)
		switch (c) {
		case 'f':
			fdtfile = optarg;
			break;
		case 'G':
			params.keyfile = optarg;
			break;
		case 'k':
			params.keydir = optarg;
			break;
		case 'K':
			params.keydest = optarg;
			break;
		case 'o':
			params.outfile = optarg;
			break;
		case 'q':
			params.quiet = true;
			break;
		case 'r':
			params.require_keys = true;
			break;
		case 'S':
			params.keyname = optarg;
			break;
		default:
			usage(cmdname);
			break;
	}

	if (!fdtfile) {
		fprintf(stderr, "%s: Missing fdt file\n", *argv);
		usage(*argv);
	}
	if (!params.keyfile) {
		fprintf(stderr, "%s: Missing key file\n", *argv);
		usage(*argv);
	}

	ret = do_fdt_sign(&params, cmdname, fdtfile);

	exit(ret);
}
