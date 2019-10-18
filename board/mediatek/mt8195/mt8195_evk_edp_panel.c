// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek innolux_hk173vb_01b edp driver
 *
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Tommy Chen <tommyyl.chen@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <asm/gpio.h>
#include "mtk_edp_panel.h"

void edp_panel_get_desc(struct edp_panel_description **edp_panel_desc);

struct display_mode innolux_hk173vb_01b = {
	.clock = 533280,
};

static void innolux_hk173vb_01b_power_on(void)
{
	struct edp_panel_description *edp_panel_desc = NULL;

	edp_panel_get_desc(&edp_panel_desc);

	if (dm_gpio_is_valid(&edp_panel_desc->vcc_gpio) &&
	    dm_gpio_is_valid(&edp_panel_desc->bl_en_gpio) &&
	    dm_gpio_is_valid(&edp_panel_desc->bl_vcc_gpio) &&
	    dm_gpio_is_valid(&edp_panel_desc->pwm_gpio)) {
		dm_gpio_set_value(&edp_panel_desc->vcc_gpio, 1);
		dm_gpio_set_value(&edp_panel_desc->bl_en_gpio, 1);
		dm_gpio_set_value(&edp_panel_desc->bl_vcc_gpio, 1);
		dm_gpio_set_value(&edp_panel_desc->pwm_gpio, 1);
	} else {
		printf("Warning: %s some gpios invalid. could not power on panel\n", __func__);
	}
}

struct edp_panel_description innolux_hk173vb_01b_desc = {
	.name = "innolux_hk173vb_01b",
	.compatible = "innolux,hk173vb-01b",
	.mode = &innolux_hk173vb_01b,
	.power_on = innolux_hk173vb_01b_power_on,
};

void edp_panel_get_desc(struct edp_panel_description **edp_panel_desc)
{
	*edp_panel_desc = &innolux_hk173vb_01b_desc;
}
