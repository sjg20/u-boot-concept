// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Copyright (c) 2023, Svyatoslav Ryhel <clamor95@gmail.com>
 */

/* T114 Transformers derive from Macallan board */

#include <common.h>
#include <dm.h>
#include <env.h>
#include <fdt_support.h>
#include <i2c.h>
#include <log.h>
#include <asm/arch/pinmux.h>
#include <asm/arch/gp_padctrl.h>
#include <asm/arch-tegra/fuse.h>
#include <linux/delay.h>

#include "pinmux-config-transformer.h"

#define TPS65913_I2C_ADDR		0x58

#define TPS65913_SMPS9_CTRL		0x38
#define TPS65913_SMPS9_VOLTAGE		0x3B
#define TPS65913_LDO9_CTRL		0x60
#define TPS65913_LDO9_VOLTAGE		0x61
#define TPS65913_LDOUSB_CTRL		0x64
#define TPS65913_LDOUSB_VOLTAGE		0x65

#define TPS65913_DEV_CTRL		0xA0
#define TPS65913_INT3_MASK		0x1B
#define TPS65913_INT3_MASK_VBUS		BIT(7)

#ifdef CONFIG_CMD_POWEROFF
int do_poweroff(struct cmd_tbl *cmdtp, int flag,
		int argc, char *const argv[])
{
	struct udevice *dev;
	int ret;

	/* Mask INT3 on second page first */
	ret = i2c_get_chip_for_busnum(0, TPS65913_I2C_ADDR + 1, 1, &dev);
	if (ret) {
		log_debug("cannot find PMIC I2C chip\n");
		return 1;
	}

	ret = dm_i2c_reg_read(dev, TPS65913_INT3_MASK);
	if (ret < 0)
		return ret;

	ret = dm_i2c_reg_write(dev, TPS65913_INT3_MASK,
			       ret | TPS65913_INT3_MASK_VBUS);
	if (ret)
		return ret;

	ret = i2c_get_chip_for_busnum(0, TPS65913_I2C_ADDR, 1, &dev);
	if (ret) {
		log_debug("cannot find PMIC I2C chip\n");
		return 1;
	}

	/* TPS65913: DEV_CTRL > OFF */
	ret = dm_i2c_reg_write(dev, TPS65913_DEV_CTRL, 0);
	if (ret)
		log_debug("PMU i2c_write DEV_CTRL < OFF returned %d\n", ret);

	// wait some time and then print error
	mdelay(5000);
	printf("Failed to power off!!!\n");
	return 1;
}
#endif

/*
 * Routine: pinmux_init
 * Description: Do individual peripheral pinmux configs
 */
void pinmux_init(void)
{
	pinmux_config_pingrp_table(tegra114_pinmux_common,
		ARRAY_SIZE(tegra114_pinmux_common));

	/* Initialize any non-default pad configs (APB_MISC_GP regs) */
	pinmux_config_drvgrp_table(transformer_t114_padctrl,
		ARRAY_SIZE(transformer_t114_padctrl));
}

#ifdef CONFIG_MMC_SDHCI_TEGRA
static void tps65913_voltage_init(void)
{
	struct udevice *dev;
	int ret;

	ret = i2c_get_chip_for_busnum(0, TPS65913_I2C_ADDR, 1, &dev);
	if (ret) {
		log_debug("cannot find PMIC I2C chip\n");
		return;
	}

	/* TPS65913: SMPS9_VOLTAGE = 2.9V */
	ret = dm_i2c_reg_write(dev, TPS65913_SMPS9_VOLTAGE, 0xE5);
	if (ret)
		log_debug("PMU i2c_write SMPS9 < 2.9v returned %d\n", ret);

	/* TPS65913: SMPS9_CTRL = Active */
	ret = dm_i2c_reg_write(dev, TPS65913_SMPS9_CTRL, BIT(0));
	if (ret)
		log_debug("SMPS9 enable returned %d\n", ret);

	/* TPS65913: LDO9_VOLTAGE = 2.9V */
	ret = dm_i2c_reg_write(dev, TPS65913_LDO9_VOLTAGE, 0x29);
	if (ret)
		log_debug("PMU i2c_write LDO9 < 2.9v returned %d\n", ret);

	/* TPS65913: LDO9_CTRL = Active */
	ret = dm_i2c_reg_write(dev, TPS65913_LDO9_CTRL, BIT(0));
	if (ret)
		log_debug("LDO9 enable returned %d\n", ret);

	/* TPS65913: LDOUSB_VOLTAGE = 3.3V */
	ret = dm_i2c_reg_write(dev, TPS65913_LDOUSB_VOLTAGE, 0x31);
	if (ret)
		log_debug("PMU i2c_write LDOUSB < 3.3v returned %d\n", ret);

	/* TPS65913: LDOUSB_CTRL = Active */
	ret = dm_i2c_reg_write(dev, TPS65913_LDOUSB_CTRL, BIT(0));
	if (ret)
		log_debug("LDOUSB enable returned %d\n", ret);
}

/*
 * Routine: pin_mux_mmc
 * Description: setup the MMC muxes, power rails, etc.
 */
void pin_mux_mmc(void)
{
	/* Bring up uSD and eMMC power */
	tps65913_voltage_init();
}
#endif /* MMC */

#if defined(CONFIG_OF_LIBFDT) && defined(CONFIG_OF_BOARD_SETUP)
int ft_board_setup(void *blob, struct bd_info *bd)
{
	/* Remove TrustZone nodes */
	fdt_del_node_and_alias(blob, "/firmware");
	fdt_del_node_and_alias(blob, "/reserved-memory/trustzone@bfe00000");

	return 0;
}
#endif

void nvidia_board_late_init(void)
{
	char serialno_str[17];

	/* Set chip id as serialno */
	sprintf(serialno_str, "%016llx", tegra_chip_uid());
	env_set("serial#", serialno_str);
	env_set("platform", "Tegra 4 T114");
}
