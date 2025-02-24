// SPDX-License-Identifier: GPL-2.0+
/*
 * Implementation of the logic to perform a boot
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#include <bootctl.h>
#include <dm.h>
#include <version.h>
#include <dm/device-internal.h>
#include "util.h"
#include "display.h"
#include "oslist.h"

int bootctl_get_dev(enum bootctl_type type, struct udevice **devp)
{
	struct udevice *dev;
	struct uclass *uc;

	uclass_id_foreach_dev(UCLASS_BOOTCTL, dev, uc) {
		struct bootctl_uc_plat *ucp = dev_get_uclass_plat(dev);

		if (type == ucp->type) {
			int ret;

			*devp = dev;
			ret = device_probe(dev);
			if (ret)
				return ret;
			return 0;
		}
	}

	return -ENOENT;
}

int bootctl_run(void)
{
	struct udevice *disp, *oslist;
	struct osinfo *selected;
	struct oslist_iter iter;
	bool running, scanning;

	/* figure out the display to use */
	LOGR("bgd", bootctl_get_dev(BOOTCTLT_DISPLAY, &disp));
	bc_printf(disp, "Canonical Sourceboot v%d.%02d\n",
		  U_BOOT_VERSION_NUM, U_BOOT_VERSION_NUM_PATCH);

	/* figure out the oslist to use */
	LOGR("bgo", bootctl_get_dev(BOOTCTLT_OSLIST, &oslist));

	LOGR("bds", bc_display_show(disp));

	running = false;
	scanning = false;
	selected = NULL;
	do {
		struct osinfo info;
		int ret = -ENOENT;

		if (!running) {
			ret = bc_oslist_first(oslist, &iter, &info);
			running = true;
			scanning = !ret;
		} else if (scanning) {
			ret = bc_oslist_next(oslist, &iter, &info);
			if (ret)
				scanning = false;
		}

		if (!ret)
			LOGR("bda", bc_display_add(disp, &info));

		LOGR("bdr", bc_display_render(disp));
		ret = bc_display_poll(disp, &selected);
		if (!ret)
			running = false;
	} while (running);

	if (selected) {
		printf("boot\n");
	}

	return 0;
}
