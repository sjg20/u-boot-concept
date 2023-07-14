// SPDX-License-Identifier: GPL-2.0+
/*
 *  (C) Copyright 2010-2013
 *  NVIDIA Corporation <www.nvidia.com>
 *
 *  (C) Copyright 2021
 *  Svyatoslav Ryhel <clamor95@gmail.com>
 */

/* T30 Transformers derive from Cardhu board */

#include <common.h>
#include <dm.h>
#include <env.h>
#include <fdt_support.h>
#include <asm/arch/pinmux.h>
#include <asm/arch/gp_padctrl.h>
#include <asm/arch/gpio.h>
#include <asm/arch-tegra/fuse.h>
#include <asm/gpio.h>

#include "pinmux-config-transformer.h"

#include <dm.h>
#include <i2c.h>
#include <log.h>
#include <linux/delay.h>

#define TPS65911_I2C_ADDRESS		0x2D

#define TPS65911_VDD1			0x21
#define TPS65911_VDD1_OP		0x22
#define TPS65911_LDO1			0x30
#define TPS65911_LDO2			0x31
#define TPS65911_LDO3			0x37
#define TPS65911_LDO5			0x32
#define TPS65911_LDO6			0x35

#define TPS65911_GPIO0			0x60
#define TPS65911_GPIO6			0x66
#define TPS65911_GPIO7			0x67
#define TPS65911_GPIO8			0x68

#define TPS65911_DEVCTRL		0x3F
#define   DEVCTRL_PWR_OFF_MASK		BIT(7)
#define   DEVCTRL_DEV_ON_MASK		BIT(2)
#define   DEVCTRL_DEV_OFF_MASK		BIT(0)

#ifdef CONFIG_CMD_POWEROFF
int do_poweroff(struct cmd_tbl *cmdtp, int flag,
		int argc, char *const argv[])
{
	struct udevice *dev;
	uchar data_buffer[1];
	int ret;

	ret = i2c_get_chip_for_busnum(0, TPS65911_I2C_ADDRESS, 1, &dev);
	if (ret) {
		log_debug("cannot find PMIC I2C chip\n");
		return 0;
	}

	ret = dm_i2c_read(dev, TPS65911_DEVCTRL, data_buffer, 1);
	if (ret)
		return ret;

	data_buffer[0] |= DEVCTRL_PWR_OFF_MASK;

	ret = dm_i2c_write(dev, TPS65911_DEVCTRL, data_buffer, 1);
	if (ret)
		return ret;

	data_buffer[0] |= DEVCTRL_DEV_OFF_MASK;
	data_buffer[0] &= ~DEVCTRL_DEV_ON_MASK;

	ret = dm_i2c_write(dev, TPS65911_DEVCTRL, data_buffer, 1);
	if (ret)
		return ret;

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
	pinmux_config_pingrp_table(transformer_pinmux_common,
		ARRAY_SIZE(transformer_pinmux_common));

	pinmux_config_drvgrp_table(transformer_padctrl,
		ARRAY_SIZE(transformer_padctrl));

	if (of_machine_is_compatible("asus,tf700t")) {
		pinmux_config_pingrp_table(tf700t_mipi_pinmux,
			ARRAY_SIZE(tf700t_mipi_pinmux));
	}
}

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
	env_set("platform", "Tegra 3 T30");
}
