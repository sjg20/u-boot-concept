// SPDX-License-Identifier: GPL-2.0+
/*
 * Bootmethod for distro boot
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootmethod.h>
#include <dm.h>
#include <mmc.h>

int distro_setup(struct udevice *dev, struct bootflow *bflow)
{
	return 0;
}

struct bootmethod_ops distro_bmethod_ops = {
	.setup	= distro_setup,
};

U_BOOT_DRIVER(distro_bmethod) = {
	.name		= "distro_bmethod",
	.id		= UCLASS_BOOTMETHOD,
	.ops		= &distro_bmethod_ops,
};
