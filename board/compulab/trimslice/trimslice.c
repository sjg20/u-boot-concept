/*
 *  (C) Copyright 2010-2012
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
#include <netdev.h>
#include <i2c.h>
#include <asm/io.h>
#include <asm/arch/tegra2.h>
#include <asm/arch/gpio.h>
#include <asm/arch/clock.h>
#include <asm/arch/funcmux.h>
#include <asm/arch/pinmux.h>
#include <asm/arch/mmc.h>
#include <asm/gpio.h>
#ifdef CONFIG_TEGRA_MMC
#include <mmc.h>
#endif

/*
 * Routine: gpio_config_uart
 * Description: Does nothing on TrimSlice - no UART-related GPIOs.
 */
void gpio_config_uart(void)
{
}

void pin_mux_spi(void)
{
	funcmux_select(PERIPH_ID_SPI1, FUNCMUX_SPI1_GMC_GMD);
}

/*
 * Routine: pin_mux_mmc
 * Description: setup the pin muxes/tristate values for the SDMMC(s)
 */
static void pin_mux_mmc(void)
{
	funcmux_select(PERIPH_ID_SDMMC1, FUNCMUX_SDMMC1_SDIO1_4BIT);
	funcmux_select(PERIPH_ID_SDMMC4, FUNCMUX_SDMMC4_ATB_GMA_4_BIT);

	/* For CD GPIO PP1 */
	pinmux_tristate_disable(PINGRP_DAP3);
}

/* this is a weak define that we are overriding */
int board_mmc_init(bd_t *bd)
{
	debug("board_mmc_init called\n");

	/* Enable muxes, etc. for SDMMC controllers */
	pin_mux_mmc();

	/* init dev 0 (SDMMC4), (micro-SD slot) with 4-bit bus */
	tegra2_mmc_init(0, 4, -1, GPIO_PP1);

	/* init dev 3 (SDMMC1), (SD slot) with 4-bit bus */
	tegra2_mmc_init(3, 4, -1, -1);

	return 0;
}

int board_eth_init(bd_t *bis)
{
	return pci_eth_init(bis);
}

extern int tegra_pcie_init(int init_port0, int init_port1);
void pci_init_board(void)
{
	pinmux_set_func(PINGRP_GPV, PMUX_FUNC_PCIE);
	pinmux_tristate_disable(PINGRP_GPV);
	tegra_pcie_init(1, 0);
}

static struct gpio_desc {
	unsigned int gpio;
	unsigned int pin_group;
	unsigned int value;
	char     name[16];
} gpios[] = {
	{GPIO_PV2, PINGRP_UAC,  1, "USB1_MUX_SEL"},
	{GPIO_PV3, PINGRP_UAC,  0, "USB1_VBUS_EN"},
	{GPIO_PA3, PINGRP_DAP2, 1, "SATA_nRST"}
};

static inline void board_gpio_set(struct gpio_desc *gpio_desc)
{
	pinmux_tristate_disable(gpio_desc->pin_group);
	if (gpio_request(gpio_desc->gpio, gpio_desc->name)) {
		printf("gpio: requesting pin %u failed\n", gpio_desc->gpio);
		return;
	}
	debug("gpio: setting pin %u %d (%s)\n", gpio_desc->gpio, gpio_desc->value, gpio_desc->name);
	gpio_direction_output(gpio_desc->gpio, gpio_desc->value);
	gpio_free(gpio_desc->gpio);
}

void pin_mux_usb(void)
{
	int i = 0 ;
	for (i = 0 ; i < (sizeof(gpios)/sizeof(struct gpio_desc)) ; i++)
		board_gpio_set(&gpios[i]);
}
