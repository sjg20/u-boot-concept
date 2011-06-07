/*
 *  (C) Copyright 2010
 *  NVIDIA Corporation <www.nvidia.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/arch/gpio.h>

#include "gp-util.h"

#ifdef TEGRA_PANEL_POWERON_SEQUENCE
struct tegra_panel_sequence tegra_panel_sequence_table = {
	.lvds_enable = TEGRA_PANEL_GPIO_LVDS_ENABLE,
	.backlight = TEGRA_PANEL_GPIO_BACKLIGHT,
	.backlight_vdd = TEGRA_PANEL_GPIO_BACKLIGHT_VDD,
	.panel_power_enable = TEGRA_PANEL_GPIO_EN_VDD_PNL,
	.T3 = TEGRA_PANEL_DATA_TO_BACKLIGHTPWR_MS,
	.T5 = TEGRA_PANEL_BACKLIGHTPWR_TO_VPWM_MS,
	.T6 = TEGRA_PANEL_VPWM_TO_VEN_MS,
};
#endif

static void clk_init(void)
{
	tegra_clk_common_init();
	tegra_clk_init_from_table(tegra2_gp_clk_init_table);
}

static void pinmux_init(void)
{
	tegra_pinmux_config_table(tegra2_gp_pinmux, tegra2_gp_pinmux_tab_len);
}

static void panel_init(void)
{
	int i;

	for (i = 0; i < tegra2_gp_gpio_offset_tab_len; i++) {
		tg2_gpio_direction_output_ex(
			tegra2_gp_gpio_init_table[i].offset, 1);
		if (tegra2_gp_gpio_init_table[i].set == true)
			tg2_gpio_set_value_ex(
				tegra2_gp_gpio_init_table[i].offset, 1);
	}
}

#ifdef	TEGRA_PANEL_POWERON_SEQUENCE
void mdelay(int ms)
{
	udelay(ms * 1000);
}

static void poweron_panel(void)
{
	tegra_dc_register(tegra2_gp_panel_resources,
			tegra2_gp_panel_modes,
			&tegra2_gp_fb_data);

	/* Enable Panel power */
	tg2_gpio_direction_output_ex(
		tegra_panel_sequence_table.panel_power_enable, 1);
	tg2_gpio_set_value_ex(
		tegra_panel_sequence_table.panel_power_enable, 1);

	/* Enable LVDS */
	tg2_gpio_direction_output_ex(
		tegra_panel_sequence_table.lvds_enable, 1);
	tg2_gpio_set_value_ex(
		tegra_panel_sequence_table.lvds_enable, 1);

	tegra_dc_probe();

	/* Data to Backlight Rise Time */
	mdelay(tegra_panel_sequence_table.T3);

	/* Enable Backlight power */
	tg2_gpio_direction_output_ex(
		tegra_panel_sequence_table.backlight_vdd, 1);
	tg2_gpio_set_value_ex(
		tegra_panel_sequence_table.backlight_vdd, 1);

	/* Backlight to Vpwm Rise Time */
	mdelay(tegra_panel_sequence_table.T5);

	tegra_pwm_enable();

	/* Vpwm to Ven Rise Time */
	mdelay(tegra_panel_sequence_table.T6);

	/* Enable Ven(LCD enable signal) */
	tg2_gpio_direction_output_ex(tegra_panel_sequence_table.backlight, 1);
	tg2_gpio_set_value_ex(tegra_panel_sequence_table.backlight, 1);
}
#endif

void gpinit(void)
{
	clk_init();
	pinmux_init();
	poweron_3d();

#ifdef	TEGRA_PANEL_POWERON_SEQUENCE
	/*
	In the poweron_panel(), we must conform to the panel specification
	to complete the power-on sequence and required delays exactly.
	Most of panels have the similar sequence, you could modify required
	delays in your case.
	*/
	poweron_panel();
#else
	panel_init();
	tegra_dc_register(tegra2_gp_panel_resources,
			tegra2_gp_panel_modes,
			&tegra2_gp_fb_data);
	tegra_pwm_enable();
	tegra_dc_probe();
#endif
}
