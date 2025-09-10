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
#include <bootmeth.h>
#include <bootstd.h>
#include <dm/device.h>
#include <sandbox_host.h>
#include <u-boot-api.h>

static void show_bootflow(int num, struct bootflow *bflow)
{
	ub_printf("Bootflow %d:\n", num);
	ub_printf("  name: '%s'\n", bflow->name ? bflow->name : "(null)");
	ub_printf("  state: %s\n", bootflow_state_get_name(bflow->state));
	ub_printf("  method: '%s'\n",
		  bflow->method ? bflow->method->name : "(null)");
	ub_printf("  fname: '%s'\n", bflow->fname ? bflow->fname : "(null)");
	ub_printf("  dev: '%s'\n", bflow->dev ? bflow->dev->name : "(null)");
	ub_printf("  part: %d\n", bflow->part);
	ub_printf("  size: %d\n", bflow->size);
	ub_printf("  err: %d\n", bflow->err);
	if (bflow->os_name)
		ub_printf("  os_name: '%s'\n", bflow->os_name);
	if (bflow->logo)
		ub_printf("  logo: present (%zu bytes)\n", bflow->logo_size);
	ub_printf("\n");
}

int bootflow_internal_scan(void)
{
	struct bootflow bflow;
	struct bootflow_iter iter;
	struct bootstd_priv *std;
	struct bootflow *first_bflow;
	struct udevice *host_dev;
	int ret, count = 0;

	ub_printf("Internal bootflow scan using U-Boot headers first\n");

	/* Get bootstd private data */
	ret = bootstd_get_priv(&std);
	if (ret) {
		ub_printf("bootstd_get_priv() failed: %d\n", ret);
		return ret;
	}

	/* Set bootmethod order to only use extlinux and efi */
	ret = bootmeth_set_order("extlinux efi");
	if (ret) {
		ub_printf("bootmeth_set_order() failed: %d\n", ret);
		return ret;
	}
	ub_printf("Set bootmethod order to: extlinux efi\n");

	/* Now we can actually use bootflow.h definitions! */
	ub_printf("BOOTFLOWST_MEDIA = %d\n", BOOTFLOWST_MEDIA);
	ub_printf("sizeof(struct bootflow) = %zu\n", sizeof(struct bootflow));
	ub_printf("sizeof(struct bootflow_iter) = %zu\n",
		  sizeof(struct bootflow_iter));

	/* Attach the MMC image file to make bootflows available */
	ub_printf("Attaching mmc1.img file...\n");
	ret = host_create_attach_file("mmc1", "/home/sglass/u/mmc1.img", false,
				      512, &host_dev);
	if (ret) {
		ub_printf("host_create_attach_file() failed: %d\n", ret);

		return ret;
	}

	/* List all available bootdevs */
	ub_printf("Available bootdevs:\n");
	bootdev_list(true);

	/* Try to scan for the first bootflow */
	ret = bootflow_scan_first(NULL, NULL, &iter, BOOTFLOWIF_SHOW,
				  &bflow);
	if (ret) {
		ub_printf("bootflow_scan_first() failed: %d\n", ret);
		return ret;
	}

	/* Iterate through all bootflows */
	do {
		count++;
		show_bootflow(count, &bflow);

		/* Add bootflow to the global list */
		ret = bootstd_add_bootflow(&bflow);
		if (ret < 0) {
			ub_printf("bootstd_add_bootflow() failed: %d\n", ret);
			bootflow_free(&bflow);
		}

		/* Get next bootflow */
		ret = bootflow_scan_next(&iter, &bflow);
	} while (!ret);

	ub_printf("Found %d total bootflows\n", count);

	/* Clean up the iterator */
	bootflow_iter_uninit(&iter);

	/* Return immediately if no bootflows found */
	if (!count) {
		ub_printf("No bootflows found to boot\n");
		return 0;
	}

	/* Boot the first bootflow */
	/* Get the first bootflow from the global list */
	first_bflow = alist_getw(&std->bootflows, 0, struct bootflow);
	if (!first_bflow) {
		ub_printf("Failed to get first bootflow from global list\n");
		return -1;
	}

	ub_printf("\bBooting: %s\n",
		  first_bflow->name ? first_bflow->name : "(unnamed)");
	if (first_bflow->os_name)
		ub_printf("OS: %s\n", first_bflow->os_name);

	ret = bootflow_boot(first_bflow);
	if (ret)
		ub_printf("bootflow_boot() failed: %dE\n", ret);
	else
		ub_printf("bootflow_boot() succeeded (shouldn't reach here!)\n");

	return 0;
}
