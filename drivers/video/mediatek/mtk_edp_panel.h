/* SPDX-License-Identifier: GPL-2.0
 *
 * MediaTek common dsi panel header
 *
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Tommy Chen <tommyyl.chen@mediatek.com>
 */

#ifndef __MTK_EDP_PANEL_H__
#define __MTK_EDP_PANEL_H__

#include <linux/io.h>
#include <linux/bitops.h>
#include <asm/gpio.h>
#include <clk.h>
#include "mtk_dp_intf.h"

struct edp_panel_description {
	const char *name;  /* Panel name for constructing CBFS file name */
	const char *compatible;  /* Panel compatible string in kernel dts */
	struct display_mode *mode;
	struct gpio_desc vcc_gpio;
	struct gpio_desc bl_en_gpio;
	struct gpio_desc bl_vcc_gpio;
	struct gpio_desc pwm_gpio;
	void (*power_on)(void);  /* Callback to turn on panel */
	void (*power_off)(void);  /* Callback to turn off panel */
};

#endif /* __MTK_EDP_PANEL_H__ */
