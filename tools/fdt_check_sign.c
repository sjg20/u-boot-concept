// SPDX-License-Identifier: GPL-2.0+
/*
 * Check a signature in an FDT file
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
		"Usage: %s -f dtb_file -k key file\n"
		"          -f ==> set dtb file which should be checked\n"
		"          -k ==> set key .dtb file which should be checked\n",
		cmdname);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	struct image_tool_params params;
	char *fdtfile = NULL;
	char *keyfile = NULL;
	char cmdname[256];
	struct stat fsbuf;
	struct stat ksbuf;
	void *fdt_blob;
	void *key_blob;
	int ffd = -1;
	int kfd = -1;
	int ret;
	int c;

	memset(&params, '\0', sizeof(params));
	strncpy(cmdname, *argv, sizeof(cmdname) - 1);
	cmdname[sizeof(cmdname) - 1] = '\0';
	while ((c = getopt(argc, argv, "f:k:")) != -1)
		switch (c) {
		case 'f':
			fdtfile = optarg;
			break;
		case 'k':
			keyfile = optarg;
			break;
		default:
			usage(cmdname);
			break;
	}

	if (!fdtfile) {
		fprintf(stderr, "%s: Missing fdt file\n", *argv);
		usage(*argv);
	}
	if (!keyfile) {
		fprintf(stderr, "%s: Missing key file\n", *argv);
		usage(*argv);
	}

	ffd = mmap_fdt(cmdname, fdtfile, 0, &fdt_blob, &fsbuf, false, true);
	if (ffd < 0)
		return EXIT_FAILURE;
	kfd = mmap_fdt(cmdname, keyfile, 0, &key_blob, &ksbuf, false, true);
	if (kfd < 0)
		return EXIT_FAILURE;

	ret = fdt_check_sign(fdt_blob, key_blob);
	if (!ret) {
		ret = EXIT_SUCCESS;
		printf("Signature check OK\n");
	} else {
		ret = EXIT_FAILURE;
		fprintf(stderr, "Signature check bad (error %d)\n", ret);
	}

	(void)munmap((void *)fdt_blob, fsbuf.st_size);
	(void)munmap((void *)key_blob, ksbuf.st_size);

	close(ffd);
	close(kfd);
	exit(ret);
}
