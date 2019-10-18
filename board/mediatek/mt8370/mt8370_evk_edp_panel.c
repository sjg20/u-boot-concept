// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek auo_g156han03 edp panel driver
 *
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Tommy Chen <tommyyl.chen@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <asm/gpio.h>
#include "mtk_edp_panel.h"

void edp_panel_get_desc(struct edp_panel_description **edp_panel_desc);

struct display_mode auo_g156han03 = {
	.clock = 140910,
};

static void auo_g156han03_power_on(void)
{
	struct edp_panel_description *edp_panel_desc = NULL;

	edp_panel_get_desc(&edp_panel_desc);

	if (dm_gpio_is_valid(&edp_panel_desc->vcc_gpio) &&
	    dm_gpio_is_valid(&edp_panel_desc->bl_en_gpio) &&
	    dm_gpio_is_valid(&edp_panel_desc->pwm_gpio)) {
		dm_gpio_set_value(&edp_panel_desc->vcc_gpio, 1);
		dm_gpio_set_value(&edp_panel_desc->bl_en_gpio, 1);
		dm_gpio_set_value(&edp_panel_desc->pwm_gpio, 1);
	} else {
		printf("Warning: %s some gpios invalid. could not power on panel\n", __func__);
	}
}

struct edp_panel_description auo_g156han03_desc = {
	.name = "auo_g156han03",
	.compatible = "auo,g156han03",
	.mode = &auo_g156han03,
	.power_on = auo_g156han03_power_on,
};

void edp_panel_get_desc(struct edp_panel_description **edp_panel_desc)
{
	*edp_panel_desc = &auo_g156han03_desc;
}
