// SPDX-License-Identifier: GPL-2.0+
/*
 * AXP313A driver based on AXP221 driver
 *
 *
 * (C) Copyright 2023 SASANO Takayoshi <uaa@uaa.org.uk>
 *
 * Based on axp221.c
 * (C) Copyright 2014 Hans de Goede <hdegoede@redhat.com>
 * (C) Copyright 2013 Oliver Schinagl <oliver@schinagl.nl>
 */

#include <common.h>
#include <command.h>
#include <errno.h>
#include <asm/arch/pmic_bus.h>
#include <axp_pmic.h>

static u8 axp313a_mvolt_to_cfg(int mvolt, int min, int max, int div)
{
	if (mvolt < min)
		mvolt = min;
	else if (mvolt > max)
		mvolt = max;

	return (mvolt - min) / div;
}

int axp_set_dcdc1(unsigned int mvolt)
{
	int ret;
	u8 cfg;

	if (mvolt >= 1600)
		cfg = 88 + axp313a_mvolt_to_cfg(mvolt, 1600, 3400, 100);
	else if (mvolt >= 1220)
		cfg = 71 + axp313a_mvolt_to_cfg(mvolt, 1220, 1540, 20);
	else
		cfg = axp313a_mvolt_to_cfg(mvolt, 500, 1200, 10);

	if (mvolt == 0)
		return pmic_bus_clrbits(AXP313A_OUTPUT_CTRL,
					AXP313A_OUTPUT_CTRL_DCDC1);

	ret = pmic_bus_write(AXP313A_DCDC1_CTRL, cfg);
	if (ret)
		return ret;

	return pmic_bus_setbits(AXP313A_OUTPUT_CTRL,
				AXP313A_OUTPUT_CTRL_DCDC1);
}

int axp_set_dcdc2(unsigned int mvolt)
{
	int ret;
	u8 cfg;

	if (mvolt >= 1220)
		cfg = 71 + axp313a_mvolt_to_cfg(mvolt, 1220, 1540, 20);
	else
		cfg = axp313a_mvolt_to_cfg(mvolt, 500, 1200, 10);

	if (mvolt == 0)
		return pmic_bus_clrbits(AXP313A_OUTPUT_CTRL,
					AXP313A_OUTPUT_CTRL_DCDC2);

	ret = pmic_bus_write(AXP313A_DCDC2_CTRL, cfg);
	if (ret)
		return ret;

	return pmic_bus_setbits(AXP313A_OUTPUT_CTRL,
				AXP313A_OUTPUT_CTRL_DCDC2);
}

int axp_set_dcdc3(unsigned int mvolt)
{
	int ret;
	u8 cfg;

	if (mvolt >= 1220)
		cfg = 71 + axp313a_mvolt_to_cfg(mvolt, 1220, 1840, 20);
	else
		cfg = axp313a_mvolt_to_cfg(mvolt, 500, 1200, 10);

	if (mvolt == 0)
		return pmic_bus_clrbits(AXP313A_OUTPUT_CTRL,
					AXP313A_OUTPUT_CTRL_DCDC3);

	ret = pmic_bus_write(AXP313A_DCDC3_CTRL, cfg);
	if (ret)
		return ret;

	return pmic_bus_setbits(AXP313A_OUTPUT_CTRL,
				AXP313A_OUTPUT_CTRL_DCDC3);
}

int axp_set_aldo1(unsigned int mvolt)
{
	int ret;
	u8 cfg = axp313a_mvolt_to_cfg(mvolt, 500, 3500, 100);

	if (mvolt == 0)
		return pmic_bus_clrbits(AXP313A_OUTPUT_CTRL,
					AXP313A_OUTPUT_CTRL_ALDO1);

	ret = pmic_bus_write(AXP313A_ALDO1_CTRL, cfg);
	if (cfg)
		return ret;

	return pmic_bus_setbits(AXP313A_OUTPUT_CTRL,
				AXP313A_OUTPUT_CTRL_ALDO1);
}

int axp_set_dldo(int dldo_num, unsigned int mvolt)
{
	int ret;
	u8 cfg = axp313a_mvolt_to_cfg(mvolt, 500, 3500, 100);

	if (dldo_num != 1)
		return -EINVAL;

	if (mvolt == 0)
		return pmic_bus_clrbits(AXP313A_OUTPUT_CTRL,
					AXP313A_OUTPUT_CTRL_DLDO1);

	ret = pmic_bus_write(AXP313A_DLDO1_CTRL, cfg);
	if (cfg)
		return ret;

	return pmic_bus_setbits(AXP313A_OUTPUT_CTRL,
				AXP313A_OUTPUT_CTRL_DLDO1);
}

int axp_init(void)
{
	int ret;
	u8 axp_chip_id;

	ret = pmic_bus_init();
	if (ret)
		return ret;

	ret = pmic_bus_read(AXP313A_CHIP_VERSION, &axp_chip_id);
	if (ret)
		return ret;

	axp_chip_id &= AXP313A_CHIP_VERSION_MASK;
	switch (axp_chip_id) {
	case AXP313A_CHIP_VERSION_AXP1530:
	case AXP313A_CHIP_VERSION_AXP313A:
	case AXP313A_CHIP_VERSION_AXP313B:
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

#if !IS_ENABLED(CONFIG_SYSRESET_CMD_POWEROFF)
int do_poweroff(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	pmic_bus_write(AXP313A_SHUTDOWN, AXP313A_POWEROFF);

	/* infinite loop during shutdown */
	while (1);

	/* not reached */
	return 0;
}
#endif
