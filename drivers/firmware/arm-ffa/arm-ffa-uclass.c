// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2022-2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * Authors:
 *   Abdellatif El Khlifi <abdellatif.elkhlifi@arm.com>
 */

#include <common.h>
#include <dm.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

/**
 * ffa_bus_get_ops() - bus driver operations getter
 * @dev:	the arm_ffa device
 * Returns a pointer to the FF-A driver ops field.
 * Return:
 * The ops pointer on success, NULL on failure.
 */
const struct ffa_bus_ops *ffa_bus_get_ops(struct udevice *dev)
{
	if (!dev)
		return NULL;

	return device_get_ops(dev);
}

UCLASS_DRIVER(ffa) = {
	.name		= "ffa",
	.id		= UCLASS_FFA,
};
