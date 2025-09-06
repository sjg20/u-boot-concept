// SPDX-License-Identifier: GPL-2.0+
/*
 * Demo program showing U-Boot library functionality
 *
 * This demonstrates using U-Boot library functions in sandbox like os_*
 * from external programs.
 *
 * Copyright 2025 Canonical Ltd.
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <os.h>
#include <u-boot-lib.h>
#include <version_string.h>
#include "demo_helper.h"

int main(int argc, char *argv[])
{
	int fd, result, lines = 0;
	char line[256];

	/* Init U-Boot library */
	if (ulib_init(argv[0]) < 0) {
		fprintf(stderr, "Failed to initialize U-Boot library\n");
		return 1;
	}

	demo_show_banner();
	printf("U-Boot version: %s\n", version_string);
	printf("\n");

	/* Use U-Boot's os_open to open a file */
	fd = os_open("/proc/version", 0);
	if (fd < 0) {
		fprintf(stderr, "Failed to open /proc/version\n");
		ulib_uninit();
		return 1;
	}

	printf("System version:\n");

	/* Use U-Boot's os_fgets to read lines */
	while (os_fgets(line, sizeof(line), fd)) {
		printf("  %s", line);
		lines++;
	}

	os_close(fd);

	printf("\nRead %d line(s) using U-Boot library functions.\n", lines);

	/* Test the helper function */
	result = demo_add_numbers(42, 13);
	printf("Helper function result: %d\n", result);

	demo_show_footer();

	/* Clean up */
	ulib_uninit();

	return 0;
}
