// SPDX-License-Identifier: GPL-2.0+
/*
 *  Copyright(C) 2023 Svyatoslav Ryhel <clamor95@gmail.com>
 */

#include <common.h>
#include <fdtdec.h>
#include <errno.h>
#include <dm.h>
#include <power/pmic.h>
#include <power/regulator.h>
#include <power/max77663.h>

/* fist row is control registers, second is voltage registers */
static const char max77663_sd_reg[][MAX77663_SD_NUM] = {
	{ 0x1d, 0x1e, 0x1f, 0x20, 0x21 },
	{ 0x16, 0x17, 0x18, 0x19, 0x2a },
};

static const char max77663_ldo_reg[MAX77663_LDO_NUM] = {
	0x23, 0x25, 0x27, 0x29, 0x2b, 0x2d, 0x2f, 0x31, 0x33
};

static int max77663_sd_enable(struct udevice *dev, int op, bool *enable)
{
	struct dm_regulator_uclass_plat *uc_pdata =
					dev_get_uclass_plat(dev);
	u32 adr = uc_pdata->ctrl_reg;
	int ret;

	ret = pmic_reg_read(dev->parent, adr);
	if (ret < 0)
		return ret;

	if (op == PMIC_OP_GET) {
		if (ret & MAX77663_SD_STATUS_MASK)
			*enable = true;
		else
			*enable = false;

		return 0;
	} else if (op == PMIC_OP_SET) {
		ret &= ~(MAX77663_SD_STATUS_MASK);

		if (*enable)
			ret |= MAX77663_SD_STATUS_MASK;

		ret = pmic_reg_write(dev->parent, adr, ret);
		if (ret)
			return ret;
	}

	return 0;
}

static int max77663_sd_volt2hex(int sd, int uV)
{
	switch (sd) {
	case 0:
		/* SD0 has max voltage 1.4V */
		if (uV > MAX77663_SD0_VOLT_MAX)
			return -EINVAL;
		break;
	case 1:
		/* SD1 has max voltage 1.55V */
		if (uV > MAX77663_SD1_VOLT_MAX)
			return -EINVAL;
		break;
	default:
		/* SD2 and SD3 have max voltage 3.79V */
		if (uV > MAX77663_SD_VOLT_MAX)
			return -EINVAL;
		break;
	};

	if (uV < MAX77663_SD_VOLT_MIN)
		uV = MAX77663_SD_VOLT_MIN;

	return (uV - MAX77663_SD_VOLT_BASE) / 12500;
}

static int max77663_sd_hex2volt(int sd, int hex)
{
	switch (sd) {
	case 0:
		/* SD0 has max voltage 1.4V */
		if (hex > MAX77663_SD0_VOLT_MAX_HEX)
			return -EINVAL;
		break;
	case 1:
		/* SD1 has max voltage 1.55V */
		if (hex > MAX77663_SD1_VOLT_MAX_HEX)
			return -EINVAL;
		break;
	default:
		/* SD2 and SD3 have max voltage 3.79V */
		if (hex > MAX77663_SD_VOLT_MAX_HEX)
			return -EINVAL;
		break;
	};

	if (hex < MAX77663_SD_VOLT_MIN_HEX)
		hex = MAX77663_SD_VOLT_MIN_HEX;

	return MAX77663_SD_VOLT_BASE + hex * 12500;
}

static int max77663_sd_val(struct udevice *dev, int op, int *uV)
{
	struct dm_regulator_uclass_plat *uc_pdata =
					dev_get_uclass_plat(dev);
	u32 adr = uc_pdata->volt_reg;
	int id = dev->driver_data;
	int ret;

	if (op == PMIC_OP_GET) {
		ret = pmic_reg_read(dev->parent, adr);
		if (ret < 0)
			return ret;

		*uV = 0;

		ret = max77663_sd_hex2volt(id, ret);
		if (ret < 0)
			return ret;
		*uV = ret;

		return 0;
	}

	/* SD regulators use entire register for voltage */
	ret = max77663_sd_volt2hex(id, *uV);
	if (ret < 0)
		return ret;

	return pmic_reg_write(dev->parent, adr, ret);
}

static int max77663_sd_probe(struct udevice *dev)
{
	struct dm_regulator_uclass_plat *uc_pdata =
					dev_get_uclass_plat(dev);

	uc_pdata->type = REGULATOR_TYPE_BUCK;
	uc_pdata->ctrl_reg = max77663_sd_reg[0][dev->driver_data];
	uc_pdata->volt_reg = max77663_sd_reg[1][dev->driver_data];

	return 0;
}

static int sd_get_value(struct udevice *dev)
{
	int uV;
	int ret;

	ret = max77663_sd_val(dev, PMIC_OP_GET, &uV);
	if (ret)
		return ret;

	return uV;
}

static int sd_set_value(struct udevice *dev, int uV)
{
	return max77663_sd_val(dev, PMIC_OP_SET, &uV);
}

static int sd_get_enable(struct udevice *dev)
{
	bool enable = false;
	int ret;

	ret = max77663_sd_enable(dev, PMIC_OP_GET, &enable);
	if (ret)
		return ret;

	return enable;
}

static int sd_set_enable(struct udevice *dev, bool enable)
{
	return max77663_sd_enable(dev, PMIC_OP_SET, &enable);
}

static const struct dm_regulator_ops max77663_sd_ops = {
	.get_value  = sd_get_value,
	.set_value  = sd_set_value,
	.get_enable = sd_get_enable,
	.set_enable = sd_set_enable,
};

U_BOOT_DRIVER(max77663_sd) = {
	.name = MAX77663_SD_DRIVER,
	.id = UCLASS_REGULATOR,
	.ops = &max77663_sd_ops,
	.probe = max77663_sd_probe,
};

static int max77663_ldo_enable(struct udevice *dev, int op, bool *enable)
{
	struct dm_regulator_uclass_plat *uc_pdata =
					dev_get_uclass_plat(dev);
	u32 adr = uc_pdata->ctrl_reg;
	int ret;

	ret = pmic_reg_read(dev->parent, adr);
	if (ret < 0)
		return ret;

	if (op == PMIC_OP_GET) {
		if (ret & MAX77663_LDO_STATUS_MASK)
			*enable = true;
		else
			*enable = false;

		return 0;
	} else if (op == PMIC_OP_SET) {
		ret &= ~(MAX77663_LDO_STATUS_MASK);

		if (*enable)
			ret |= MAX77663_LDO_STATUS_MASK;

		ret = pmic_reg_write(dev->parent, adr, ret);
		if (ret)
			return ret;
	}

	return 0;
}

static int max77663_ldo_volt2hex(int ldo, int uV)
{
	switch (ldo) {
	case 0:
	case 1:
		if (uV > MAX77663_LDO01_VOLT_MAX)
			return -EINVAL;

		return (uV - MAX77663_LDO_VOLT_BASE) / 25000;
	case 4:
		if (uV > MAX77663_LDO4_VOLT_MAX)
			return -EINVAL;

		return (uV - MAX77663_LDO_VOLT_BASE) / 12500;
	default:
		if (uV > MAX77663_LDO_VOLT_MAX)
			return -EINVAL;

		return (uV - MAX77663_LDO_VOLT_BASE) / 50000;
	};
}

static int max77663_ldo_hex2volt(int ldo, int hex)
{
	if (hex > MAX77663_LDO_VOLT_MAX_HEX)
		return -EINVAL;

	switch (ldo) {
	case 0:
	case 1:
		return (hex * 25000) + MAX77663_LDO_VOLT_BASE;
	case 4:
		return (hex * 12500) + MAX77663_LDO_VOLT_BASE;
	default:
		return (hex * 50000) + MAX77663_LDO_VOLT_BASE;
	};
}

static int max77663_ldo_val(struct udevice *dev, int op, int *uV)
{
	struct dm_regulator_uclass_plat *uc_pdata =
					dev_get_uclass_plat(dev);
	u32 adr = uc_pdata->ctrl_reg;
	int id = dev->driver_data;
	int hex, ret;

	ret = pmic_reg_read(dev->parent, adr);
	if (ret < 0)
		return ret;

	if (op == PMIC_OP_GET) {
		*uV = 0;

		ret = max77663_ldo_hex2volt(id, ret & MAX77663_LDO_VOLT_MASK);
		if (ret < 0)
			return ret;

		*uV = ret;
		return 0;
	}

	hex = max77663_ldo_volt2hex(id, *uV);
	if (hex < 0)
		return hex;

	ret &= ~(MAX77663_LDO_VOLT_MASK);

	return pmic_reg_write(dev->parent, adr, ret | hex);
}

static int max77663_ldo_probe(struct udevice *dev)
{
	struct dm_regulator_uclass_plat *uc_pdata =
					dev_get_uclass_plat(dev);

	uc_pdata->type = REGULATOR_TYPE_LDO;
	uc_pdata->ctrl_reg = max77663_ldo_reg[dev->driver_data];

	return 0;
}

static int ldo_get_value(struct udevice *dev)
{
	int uV;
	int ret;

	ret = max77663_ldo_val(dev, PMIC_OP_GET, &uV);
	if (ret)
		return ret;

	return uV;
}

static int ldo_set_value(struct udevice *dev, int uV)
{
	return max77663_ldo_val(dev, PMIC_OP_SET, &uV);
}

static int ldo_get_enable(struct udevice *dev)
{
	bool enable = false;
	int ret;

	ret = max77663_ldo_enable(dev, PMIC_OP_GET, &enable);
	if (ret)
		return ret;

	return enable;
}

static int ldo_set_enable(struct udevice *dev, bool enable)
{
	return max77663_ldo_enable(dev, PMIC_OP_SET, &enable);
}

static const struct dm_regulator_ops max77663_ldo_ops = {
	.get_value  = ldo_get_value,
	.set_value  = ldo_set_value,
	.get_enable = ldo_get_enable,
	.set_enable = ldo_set_enable,
};

U_BOOT_DRIVER(max77663_ldo) = {
	.name = MAX77663_LDO_DRIVER,
	.id = UCLASS_REGULATOR,
	.ops = &max77663_ldo_ops,
	.probe = max77663_ldo_probe,
};
