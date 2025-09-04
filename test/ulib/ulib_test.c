// SPDX-License-Identifier: GPL-2.0+
/*
 * Test application for U-Boot shared library
 *
 * This demonstrates linking against libu-boot.so
 *
 * Copyright 2025 Canonical
 * Written by Simon Glass <simon.glass@canonical.com>
 */

/* Use system headers, not U-Boot headers */
#include <stdio.h>
#include <string.h>
#include <u-boot-lib.h>

int main(int argc, char *argv[])
{
	int ret;

	printf("Calling U-Boot initialization via shared library...\n");

	ret = ulib_init(argv[0]);
	if (ret)
		return 1;

	return ret;
}
