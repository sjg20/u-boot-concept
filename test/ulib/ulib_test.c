// SPDX-License-Identifier: GPL-2.0+
/*
 * Test application for U-Boot shared library
 *
 * This demonstrates linking against libu-boot.so
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

/* Use system headers, not U-Boot headers */
#include <stdio.h>
#include <string.h>
#include <init.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

int main(int argc, char *argv[])
{
	struct global_data data;
	int ret;

	printf("Calling U-Boot initialization via shared library...\n");

	/* init global data */
	memset(&data, '\0', sizeof(data));

	ret = sandbox_init(argc, argv, &data);

	/* Do pre- and post-relocation init */
	board_init_f(gd->flags);
	board_init_r(data.new_gd, 0);

	return ret;
}
