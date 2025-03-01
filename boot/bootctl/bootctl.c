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
#include "logic.h"
#include "oslist.h"
#include "state.h"
#include "ui.h"
#include "util.h"

int bootctl_get_dev(enum uclass_id type, struct udevice **devp)
{
	struct udevice *dev;

	LOGR("bfd", uclass_first_device_err(type, &dev));
	*devp = dev;

	return 0;
}

int bootctl_run(void)
{
	struct udevice *logic;
	int ret;

	printf("Canonical Sourceboot v%d.%02d\n", U_BOOT_VERSION_NUM,
	       U_BOOT_VERSION_NUM_PATCH);

	/* figure out the UI to use */
	LOGR("bgl", bootctl_get_dev(UCLASS_BOOTCTL, &logic));

	LOGR("bcl", bc_logic_start(logic));

	do {
		ret = bc_logic_poll(logic);
		if (ret)
			printf("logic err %dE\n", ret);
	} while (ret != -ESHUTDOWN);

	return 0;
}
