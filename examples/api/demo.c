// SPDX-License-Identifier: GPL-2.0+
/*
 * Demo program showing U-Boot library functionality
 *
 * This demonstrates using U-Boot library functions in sandbox like os_*
 * from external programs.
 *
 * Copyright 2025 Canonical
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "uboot_api.h"


int main(int argc, char *argv[])
{
	int fd, lines = 0;
	char line[256];

	/* Init U-Boot library */
	if (ulib_init(argv[0]) < 0) {
		fprintf(stderr, "Failed to initialize U-Boot library\n");
		return 1;
	}

	printf("U-Boot Library Demo\n");
	printf("================================\n");

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

	/* Clean up */
	ulib_uninit();

	return 0;
}
