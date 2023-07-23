// SPDX-License-Identifier: GPL-2.0+
/*
 *  Copyright(C) 2023 Svyatoslav Ryhel <clamor95@gmail.com>
 */

#include <common.h>
#include <dm.h>
#include <i2c.h>
#include <errno.h>
#include <sysreset.h>
#include <power/pmic.h>
#include <power/tps65910_pmic.h>

static int tps65910_sysreset_request(struct udevice *dev,
				     enum sysreset_t type)
{
	int val;

	val = pmic_reg_read(dev, TPS65910_REG_DEVICE_CTRL);
	if (val < 0)
		return val;

	/* define power-off to be sequential */
	val |= PWR_OFF_SEQ;

	switch (type) {
	case SYSRESET_POWER:
		pmic_reg_write(dev->parent, TPS65910_REG_DEVICE_CTRL, val);

		val |= DEV_OFF_RST;
		val &= ~DEV_ON;

		/* TPS65910: DEV_OFF_RST + ~DEV_ON > DEVICE_CTRL */
		pmic_reg_write(dev->parent, TPS65910_REG_DEVICE_CTRL, val);
		break;
	case SYSRESET_POWER_OFF:
		pmic_reg_write(dev->parent, TPS65910_REG_DEVICE_CTRL, val);

		val |= DEV_OFF;
		val &= ~DEV_ON;

		/* TPS65910: DEV_OFF + ~DEV_ON > DEVICE_CTRL */
		pmic_reg_write(dev->parent, TPS65910_REG_DEVICE_CTRL, val);
		break;
	default:
		return -EPROTONOSUPPORT;
	}

	return -EINPROGRESS;
}

static struct sysreset_ops tps65910_sysreset = {
	.request = tps65910_sysreset_request,
};

U_BOOT_DRIVER(sysreset_tps65910) = {
	.id	= UCLASS_SYSRESET,
	.name	= TPS65910_RST_DRIVER,
	.ops	= &tps65910_sysreset,
};
