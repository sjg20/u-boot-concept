// SPDX-License-Identifier: GPL-2.0+
/*
 * Bootflow internal functions using U-Boot headers
 *
 * This demonstrates functions that need direct access to U-Boot internal
 * structures and would be compiled with U-Boot headers first.
 *
 * Copyright 2025 Canonical Ltd.
 * Written by Simon Glass <simon.glass@canonical.com>
 */

/* This file uses U-Boot headers first */
#include <bootflow.h>
#include <bootdev.h>
#include <dm/device.h>
#include <sandbox_host.h>
#include <u-boot-api.h>

int bootflow_internal_scan(void)
{
	struct bootflow bflow;
	struct bootflow_iter iter;
	int ret, count = 0;
	
	ub_printf("Internal bootflow scan using U-Boot headers first\n");
	
	/* Now we can actually use bootflow.h definitions! */
	ub_printf("BOOTFLOWST_MEDIA = %d\n", BOOTFLOWST_MEDIA);
	ub_printf("sizeof(struct bootflow) = %zu\n", sizeof(struct bootflow));
	ub_printf("sizeof(struct bootflow_iter) = %zu\n", sizeof(struct bootflow_iter));
	
	/* Attach the MMC image file to make bootflows available */
	struct udevice *host_dev;
	ub_printf("Attaching mmc1.img file...\n");
	ret = host_create_attach_file("mmc1", "/home/sglass/u/mmc1.img", false, 512, &host_dev);
	if (ret) {
		ub_printf("host_create_attach_file() failed: %d\n", ret);
		/* Continue anyway - might still find other bootflows */
	} else {
		ub_printf("Successfully attached mmc1.img as device %p\n", host_dev);
	}
	
	/* List all available bootdevs */
	ub_printf("Available bootdevs:\n");
	bootdev_list(true);
	
	/* Try to scan for the first bootflow */
	ret = bootflow_scan_first(NULL, NULL, &iter, BOOTFLOWIF_SHOW, &bflow);
	if (ret) {
		ub_printf("bootflow_scan_first() failed: %d\n", ret);
		return ret;
	}
	
	/* Iterate through all bootflows */
	do {
		count++;
		ub_printf("Bootflow %d:\n", count);
		ub_printf("  name: '%s'\n", bflow.name ? bflow.name : "(null)");
		ub_printf("  state: %d\n", bflow.state);
		ub_printf("  fname: '%s'\n", bflow.fname ? bflow.fname : "(null)");
		ub_printf("  dev: %p\n", bflow.dev);
		ub_printf("  part: %d\n", bflow.part);
		ub_printf("  size: %d\n", bflow.size);
		ub_printf("  err: %d\n", bflow.err);
		if (bflow.os_name)
			ub_printf("  os_name: '%s'\n", bflow.os_name);
		if (bflow.logo)
			ub_printf("  logo: present (%zu bytes)\n", bflow.logo_size);
		ub_printf("\n");
		
		bootflow_free(&bflow);
		/* Get next bootflow */
		ret = bootflow_scan_next(&iter, &bflow);
	} while (ret == 0);
	
	ub_printf("Found %d total bootflows\n", count);
	
	/* Clean up the iterator */
	bootflow_iter_uninit(&iter);
	
	return 0;
}
