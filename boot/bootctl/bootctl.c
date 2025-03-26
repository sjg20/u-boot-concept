// SPDX-License-Identifier: GPL-2.0+
/*
 * Implementation of the logic to perform a boot
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#define LOG_CATEGORY	UCLASS_BOOTCTL

#include <bootctl.h>
#include <dm.h>
#include <hang.h>
#include <log.h>
#include <version.h>
#include <bootctl/logic.h>
#include <bootctl/oslist.h>
#include <bootctl/state.h>
#include <bootctl/ui.h>
#include <bootctl/util.h>
#include <dm/device-internal.h>

int bootctl_get_dev(enum uclass_id type, struct udevice **devp)
{
	struct udevice *dev;
	int ret;

	ret = uclass_first_device_err(type, &dev);
	if (ret)
		return log_msg_ret("bfd", ret);

	*devp = dev;

	return 0;
}

int bootctl_run(void)
{
	struct udevice *logic;
	int ret;

	printf("Canonical Sourceboot (using U-Boot v%d.%02d)\n", U_BOOT_VERSION_NUM,
	       U_BOOT_VERSION_NUM_PATCH);

	/* figure out the UI to use */
	ret = bootctl_get_dev(UCLASS_BOOTCTL, &logic);
	if (ret)
		return log_msg_ret("bgl", ret);

	ret = bc_logic_prepare(logic);
	if (ret)
		return log_msg_ret("bcl", ret);
	ret = bc_logic_start(logic);
	if (ret)
		return log_msg_ret("bcL", ret);

	do {
		ret = bc_logic_poll(logic);
		if (ret) {
			printf("logic err %dE\n", ret);
			hang();
		}
	} while (ret != -ESHUTDOWN);

	return 0;
}
