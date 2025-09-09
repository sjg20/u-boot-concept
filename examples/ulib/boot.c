// SPDX-License-Identifier: GPL-2.0+
/*
 * Boot test program using U-Boot library
 *
 * This demonstrates basic initialization and cleanup of the U-Boot library.
 * It will be used for testing bootstd functionality using ulib.
 *
 * Copyright 2025 Canonical Ltd.
 * Written by Simon Glass <simon.glass@canonical.com>
 */

/* Use system headers, not U-Boot headers */
#include <stdio.h>
#include <stdlib.h>

#include <u-boot-api.h>
#include <u-boot-lib.h>

/* Forward declaration for bootflow function */
int bootflow_internal_scan(void);

/* Forward declaration for host function (simplified) */
int host_create_attach_file(const char *label, const char *filename,
			    int removable, unsigned long blksz,
			    void *devp);

static void fatal(const char *msg)
{
	fprintf(stderr, "Error: %s\n", msg);
	exit(1);
}

static int try_boot(void)
{
	int ret;

	printf("Scanning for bootflows...\n");

	/* MMC device attachment will be done in bootflow_internal_scan() */

	ret = bootflow_internal_scan();
	if (ret) {
		printf("Internal scan failed: %d\n", ret);
		return ret;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int ret;

	ret = ulib_init(argv[0]);
	if (ret)
		fatal("Failed to init U-Boot library");

	ret = try_boot();
	if (ret)
		printf("Boot attempt failed: %d\n", ret);

	ulib_uninit();
	return ret;
}
