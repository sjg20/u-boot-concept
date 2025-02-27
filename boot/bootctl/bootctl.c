// SPDX-License-Identifier: GPL-2.0+
/*
 * Implementation of the logic to perform a boot
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#include <bootctl.h>
#include <dm.h>
#include <log.h>
#include <version.h>
#include <dm/device-internal.h>
#include "state.h"
#include "util.h"
#include "ui.h"
#include "oslist.h"

int bootctl_get_dev(enum uclass_id type, struct udevice **devp)
{
	struct udevice *dev;

	LOGR("bfd", uclass_first_device_err(type, &dev));
	*devp = dev;

	return 0;
}

int bootctl_run(void)
{
	struct udevice *disp, *oslist, *state;
	struct osinfo *selected;
	struct oslist_iter iter;
	bool running, scanning;
	int ret;

	/* figure out the UI to use */
	LOGR("bgd", bootctl_get_dev(UCLASS_BOOTCTL_UI, &disp));
	bc_printf(disp, "Canonical Sourceboot v%d.%02d\n",
		  U_BOOT_VERSION_NUM, U_BOOT_VERSION_NUM_PATCH);

	/* figure out the oslist to use */
	LOGR("bgo", bootctl_get_dev(UCLASS_BOOTCTL_OSLIST, &oslist));

	/* figure out the state to use */
	LOGR("bgs", bootctl_get_dev(UCLASS_BOOTCTL_STATE, &state));

	/* read in our state */
	ret = bc_state_load(state);
	if (ret == -EINVAL)
		log_debug("Cannot read state, starting fresh (err=%dE)\n", ret);

	LOGR("bds", bc_ui_show(disp));

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
			LOGR("bda", bc_ui_add(disp, &info));

		LOGR("bdr", bc_ui_render(disp));
		ret = bc_ui_poll(disp, &selected);
		if (!ret)
			running = false;
	} while (running);

	if (selected) {
		printf("boot\n");
	}

	return 0;
}
