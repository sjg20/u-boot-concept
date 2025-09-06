// SPDX-License-Identifier: GPL-2.0+
/*
 * Test application for U-Boot shared library
 *
 * This demonstrates linking against libu-boot.so and libu-boot.a
 *
 * Copyright 2025 Canonical Ltd.
 * Written by Simon Glass <simon.glass@canonical.com>
 */

/* Use system headers, not U-Boot headers */
#include <stdio.h>
#include <string.h>

#include <os.h>
#include <u-boot-api.h>
#include <u-boot-lib.h>

/* Runtime detection of link type using /proc/self/maps */
static const char *detect_link_type(void)
{
	char line[512];
	int fd;
	int found_libuboot = 0;

	/* Open /proc/self/maps to check loaded libraries */
	fd = os_open("/proc/self/maps", 0);
	if (fd < 0)
		return "unable to detect linkage";

	/* Read line by line to avoid boundary issues */
	while (os_fgets(line, sizeof(line), fd)) {
		if (strstr(line, "libu-boot.so")) {
			found_libuboot = 1;
			break;
		}
	}

	os_close(fd);

	/* Return appropriate message based on what we found */
	if (found_libuboot)
		return "dynamically linked (uses libu-boot.so)";
	else
		return "statically linked (uses libu-boot.a)";
}

int main(int argc, char *argv[])
{
	int ret;

	printf("Uses libc printf before ulib_init\n");

	ret = ulib_init(argv[0]);
	if (ret)
		return 1;

	ub_printf("Hello, world from ub_printf\n");
	ub_printf("\n- U-Boot\n");
	printf("another printf()\n");
	ub_printf("\nPS: This program is %s\n", detect_link_type());

	return ret;
}
