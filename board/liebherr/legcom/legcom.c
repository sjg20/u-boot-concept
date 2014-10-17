/*
 * Copyright (C) 2014 DENX Software Engineering
 * Anatolij Gustschin <agust@denx.de>
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/iomux.h>
/*#include <asm/arch/mx6q_pins.h>*/
#include <asm/arch/mx6-pins.h>
#include <asm/errno.h>
#include <asm/gpio.h>
#include <malloc.h>
#include <asm/imx-common/iomux-v3.h>
#include <asm/imx-common/mxc_i2c.h>
#include <asm/imx-common/boot_mode.h>
#include <mmc.h>
#include <fsl_esdhc.h>
#include <micrel.h>
#include <miiphy.h>
#include <netdev.h>
#include <asm/arch/crm_regs.h>
#include <i2c.h>
#include <power/pmic.h>
#include <fsl_pmic.h>
/*#include <power/pf0100.h>*/
#include <watchdog.h>

#ifndef CONFIG_MXC_SPI
#error "CONFIG_SPI must be set for this board"
#error "PLease check your config file"
#endif

#ifdef CONFIG_STATUS_LED
#include <status_led.h>
#endif

#include <linux/fb.h>
#include <ipu_pixfmt.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/mxc_hdmi.h>

DECLARE_GLOBAL_DATA_PTR;

#define UART_PAD_CTRL  (PAD_CTL_PKE | PAD_CTL_PUE |	       \
	PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED |	       \
	PAD_CTL_DSE_40ohm   | PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#define USDHC_PAD_CTRL (PAD_CTL_PKE | PAD_CTL_PUE |	       \
	PAD_CTL_PUS_47K_UP  | PAD_CTL_SPEED_LOW |	       \
	PAD_CTL_DSE_80ohm   | PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#define ENET_PAD_CTRL  (PAD_CTL_PKE | PAD_CTL_PUE |		\
	PAD_CTL_PUS_22K_UP | PAD_CTL_SPEED_MED	  |		\
	PAD_CTL_DSE_40ohm   | PAD_CTL_HYS)

#define SPI_PAD_CTRL (PAD_CTL_HYS |				\
	PAD_CTL_PUS_100K_DOWN | PAD_CTL_SPEED_MED |		\
	PAD_CTL_DSE_40ohm   | PAD_CTL_SRE_FAST)

#define BUTTON_PAD_CTRL (PAD_CTL_PKE | PAD_CTL_PUE |		\
	PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED   |		\
	PAD_CTL_DSE_40ohm   | PAD_CTL_HYS)

#define I2C_PAD_CTRL	(PAD_CTL_PKE | PAD_CTL_PUE |		\
	PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED |		\
	PAD_CTL_DSE_40ohm | PAD_CTL_HYS |			\
	PAD_CTL_ODE | PAD_CTL_SRE_FAST)

#define WEAK_PULLUP	(PAD_CTL_PUS_100K_UP |			\
	PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm | PAD_CTL_HYS |	\
	PAD_CTL_SRE_SLOW)

#define WEAK_PULLDOWN	(PAD_CTL_PUS_100K_DOWN |		\
	PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm |			\
	PAD_CTL_HYS | PAD_CTL_SRE_SLOW)

#define OUTPUT_40OHM (PAD_CTL_SPEED_MED|PAD_CTL_DSE_40ohm)

#ifdef CONFIG_STATUS_LED

void __led_set(led_id_t mask, int state)
{
	gpio_set_value(mask, !(state == STATUS_LED_ON));
}

void __led_init(led_id_t mask, int state)
{
	gpio_request(mask, "gpio_led");
	gpio_direction_output(mask, state == STATUS_LED_ON);
}

void __led_toggle(led_id_t mask)
{
	gpio_set_value(mask, !gpio_get_value(mask));
}

#endif
int dram_init(void)
{
	gd->ram_size = get_ram_size((void *)PHYS_SDRAM, PHYS_SDRAM_SIZE);

	return 0;
}

iomux_v3_cfg_t const uart_pads[] = {
	/* UART1 */
	MX6_PAD_SD3_DAT6__UART1_RX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX6_PAD_SD3_DAT7__UART1_TX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX6_PAD_EIM_D19__UART1_CTS_B | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX6_PAD_EIM_D20__UART1_RTS_B | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX6_PAD_EIM_D23__UART1_DCD_B | MUX_PAD_CTRL(UART_PAD_CTRL),
	/* UART2 */
	MX6_PAD_EIM_D26__UART2_TX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX6_PAD_EIM_D27__UART2_RX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL),
	/* UART3 */
	/*MX6_PAD_EIM_D25__UART3_RX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL),*/
	/*MX6_PAD_EIM_D24__UART3_TX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL),*/
	/* UART4 */
	MX6_PAD_CSI0_DAT12__UART4_TX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX6_PAD_CSI0_DAT13__UART4_RX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX6_PAD_CSI0_DAT16__UART4_RTS_B | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX6_PAD_CSI0_DAT17__UART4_CTS_B | MUX_PAD_CTRL(UART_PAD_CTRL),
	/* UART5 */
	MX6_PAD_CSI0_DAT14__UART5_TX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX6_PAD_CSI0_DAT15__UART5_RX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX6_PAD_CSI0_DAT18__UART5_RTS_B | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX6_PAD_CSI0_DAT19__UART5_CTS_B | MUX_PAD_CTRL(UART_PAD_CTRL),
};


#define PC MUX_PAD_CTRL(I2C_PAD_CTRL)

/* I2C1: RTC PCF8563TS, TEMP-MON ADT7461A, Touch */
struct i2c_pads_info i2c_pad_info0 = {
	.scl = {
		.i2c_mode = MX6_PAD_EIM_D21__I2C1_SCL | PC,
		.gpio_mode = MX6_PAD_EIM_D21__GPIO3_IO21 | PC,
		.gp = IMX_GPIO_NR(3, 21)
	},
	.sda = {
		.i2c_mode = MX6_PAD_EIM_D28__I2C1_SDA | PC,
		.gpio_mode = MX6_PAD_EIM_D28__GPIO3_IO28 | PC,
		.gp = IMX_GPIO_NR(3, 28)
	}
};

/* I2C2: HDMI TPD12S016 */
struct i2c_pads_info i2c_pad_info1 = {
	.scl = {
		.i2c_mode = MX6_PAD_EIM_EB2__I2C2_SCL | PC,
		.gpio_mode = MX6_PAD_EIM_EB2__GPIO2_IO30 | PC,
		.gp = IMX_GPIO_NR(2, 30)
	},
	.sda = {
		.i2c_mode = MX6_PAD_EIM_D16__I2C2_SDA | PC,
		.gpio_mode = MX6_PAD_EIM_D16__GPIO3_IO16 | PC,
		.gp = IMX_GPIO_NR(3, 16)
	}
};

/* I2C3: PMIC PF0100, EEPROM AT24C256C */
struct i2c_pads_info i2c_pad_info2 = {
	.scl = {
		.i2c_mode = MX6_PAD_EIM_D17__I2C3_SCL | PC,
		.gpio_mode = MX6_PAD_EIM_D17__GPIO3_IO17 | PC,
		.gp = IMX_GPIO_NR(3, 17)
	},
	.sda = {
		.i2c_mode = MX6_PAD_EIM_D18__I2C3_SDA | PC,
		.gpio_mode = MX6_PAD_EIM_D18__GPIO3_IO18 | PC,
		.gp = IMX_GPIO_NR(3, 18)
	}
};

static void setup_iomux_uart(void)
{
	imx_iomux_v3_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));
}

iomux_v3_cfg_t const misc_pads[] = {
	MX6_PAD_GPIO_18__ASRC_EXT_CLK	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_CSI0_MCLK__CCM_CLKO1	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_NANDF_CS2__CCM_CLKO2	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_GPIO_17__CCM_PMIC_READY	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_GPIO_0__EPIT1_OUT	| MUX_PAD_CTRL(NO_PAD_CTRL),
	/* FLEXCAN */
	MX6_PAD_SD3_CLK__FLEXCAN1_RX	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_SD3_CMD__FLEXCAN1_TX	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_SD3_DAT1__FLEXCAN2_RX	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_SD3_DAT0__FLEXCAN2_TX	| MUX_PAD_CTRL(NO_PAD_CTRL),
	/* GPIOs */
	MX6_PAD_GPIO_6__GPIO1_IO06	| MUX_PAD_CTRL(NO_PAD_CTRL),
	/*MX6_PAD_ENET_TX_EN__GPIO1_IO28	| MUX_PAD_CTRL(NO_PAD_CTRL),*/
	MX6_PAD_ENET_TXD0__GPIO1_IO30	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_NANDF_D0__GPIO2_IO00	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_NANDF_D1__GPIO2_IO01	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_NANDF_D2__GPIO2_IO02	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_NANDF_D3__GPIO2_IO03	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_EIM_CS0__GPIO2_IO23	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_EIM_CS1__GPIO2_IO24	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_EIM_OE__GPIO2_IO25	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_EIM_D29__GPIO3_IO29	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_DI0_DISP_CLK__GPIO4_IO16 | MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_DI0_PIN15__GPIO4_IO17	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_DI0_PIN2__GPIO4_IO18	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_DI0_PIN3__GPIO4_IO19	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_DI0_PIN4__GPIO4_IO20	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_DISP0_DAT10__GPIO4_IO31	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_DISP0_DAT11__GPIO5_IO05	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_DISP0_DAT12__GPIO5_IO06	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_DISP0_DAT14__GPIO5_IO08	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_DISP0_DAT18__GPIO5_IO12	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_CSI0_PIXCLK__GPIO5_IO18	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_CSI0_DATA_EN__GPIO5_IO20 | MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_CSI0_VSYNC__GPIO5_IO21	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_CSI0_DAT4__GPIO5_IO22	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_NANDF_CLE__GPIO6_IO07	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_NANDF_WP_B__GPIO6_IO09	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_NANDF_RB0__GPIO6_IO10	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_NANDF_CS0__GPIO6_IO11	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_NANDF_CS1__GPIO6_IO14	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_NANDF_CS3__GPIO6_IO16	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_EIM_BCLK__GPIO6_IO31	| MUX_PAD_CTRL(NO_PAD_CTRL),
	/*MX6_PAD_SD3_DAT5__GPIO7_IO00	| MUX_PAD_CTRL(NO_PAD_CTRL),*/
	MX6_PAD_SD3_DAT4__GPIO7_IO01	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_SD3_DAT3__GPIO7_IO07	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_SD3_RST__GPIO7_IO08	| MUX_PAD_CTRL(NO_PAD_CTRL),
	/* GPT */
	MX6_PAD_SD1_DAT0__GPT_CAPTURE1	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_SD1_CLK__GPT_CLKIN	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_SD1_CMD__GPT_COMPARE1	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_SD1_DAT3__GPT_COMPARE3	| MUX_PAD_CTRL(NO_PAD_CTRL),
	/* HDMI */
	MX6_PAD_EIM_A25__HDMI_TX_CEC_LINE | MUX_PAD_CTRL(NO_PAD_CTRL),
	/*MX6_PAD_EIM_EB2__HDMI_TX_DDC_SCL  | MUX_PAD_CTRL(NO_PAD_CTRL),*/
	/*MX6_PAD_EIM_D16__HDMI_TX_DDC_SDA  | MUX_PAD_CTRL(NO_PAD_CTRL),*/
	/* KPP */
	MX6_PAD_KEY_COL0__KEY_COL0	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_KEY_COL1__KEY_COL1	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_KEY_COL2__KEY_COL2	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_KEY_COL3__KEY_COL3	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_GPIO_19__KEY_COL5	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_CSI0_DAT6__KEY_COL6	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_KEY_ROW0__KEY_ROW0	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_KEY_ROW2__KEY_ROW2	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_KEY_ROW3__KEY_ROW3	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_KEY_ROW4__KEY_ROW4	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_CSI0_DAT5__KEY_ROW5	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_CSI0_DAT7__KEY_ROW6	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_GPIO_5__KEY_ROW7	| MUX_PAD_CTRL(NO_PAD_CTRL),
	/* PWM */
	MX6_PAD_DISP0_DAT8__PWM1_OUT	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_DISP0_DAT9__PWM2_OUT	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_SD1_DAT1__PWM3_OUT	| MUX_PAD_CTRL(NO_PAD_CTRL),
	/* USB
	MX6_PAD_EIM_D30__USB_H1_OC	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_EIM_D31__USB_H1_PWR	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_GPIO_1__USB_OTG_ID	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_KEY_COL4__USB_OTG_OC	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_EIM_D22__USB_OTG_PWR	| MUX_PAD_CTRL(NO_PAD_CTRL),
	 */
	/* WDOG1 */
	MX6_PAD_GPIO_9__WDOG1_B	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_SD1_DAT2__WDOG1_RESET_B_DEB | MUX_PAD_CTRL(NO_PAD_CTRL),
	/* XTALOSC */
	MX6_PAD_GPIO_3__XTALOSC_REF_CLK_24M | MUX_PAD_CTRL(NO_PAD_CTRL),
};

#ifdef CONFIG_FSL_ESDHC
iomux_v3_cfg_t const usdhc2_pads[] = {
	MX6_PAD_SD2_CLK__SD2_CLK   | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD2_CMD__SD2_CMD   | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD2_DAT0__SD2_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD2_DAT1__SD2_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD2_DAT2__SD2_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD2_DAT3__SD2_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_NANDF_D4__SD2_DATA4 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_NANDF_D5__SD2_DATA5 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_NANDF_D6__SD2_DATA6 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_NANDF_D7__SD2_DATA7 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_KEY_ROW1__SD2_VSELECT | MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_GPIO_2__SD2_WP   | MUX_PAD_CTRL(NO_PAD_CTRL), /* WP */
	/*MX6_PAD_GPIO_4__SD2_CD_B | MUX_PAD_CTRL(NO_PAD_CTRL), *//* CD */
	MX6_PAD_GPIO_4__GPIO1_IO04 | MUX_PAD_CTRL(NO_PAD_CTRL), /* CD */
};

iomux_v3_cfg_t const usdhc4_pads[] = {
	MX6_PAD_SD4_CLK__SD4_CLK	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD4_CMD__SD4_CMD	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD4_DAT0__SD4_DATA0	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD4_DAT1__SD4_DATA1	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD4_DAT2__SD4_DATA2	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD4_DAT3__SD4_DATA3	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD4_DAT4__SD4_DATA4	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD4_DAT5__SD4_DATA5	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD4_DAT6__SD4_DATA6	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD4_DAT7__SD4_DATA7	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_NANDF_ALE__SD4_RESET	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
};

struct fsl_esdhc_cfg usdhc_cfg[2] = {
	{ USDHC2_BASE_ADDR, 0, 4, },
	{ USDHC4_BASE_ADDR, 0, 4, },
};

int board_mmc_getcd(struct mmc *mmc)
{
	struct fsl_esdhc_cfg *cfg = (struct fsl_esdhc_cfg *)mmc->priv;

	if (cfg->esdhc_base == USDHC2_BASE_ADDR) {
		gpio_direction_input(IMX_GPIO_NR(1, 4));
		return !gpio_get_value(IMX_GPIO_NR(1, 4));
	} else
		return 1;
}

int board_mmc_init(bd_t *bis)
{
	s32 status = 0;
	int idx;

	usdhc_cfg[0].sdhc_clk = mxc_get_clock(MXC_ESDHC2_CLK);
	usdhc_cfg[1].sdhc_clk = mxc_get_clock(MXC_ESDHC4_CLK);

	for (idx = 0; idx < CONFIG_SYS_FSL_USDHC_NUM; ++idx) {
		switch (idx) {
		case 0:
			imx_iomux_v3_setup_multiple_pads(
				usdhc2_pads, ARRAY_SIZE(usdhc2_pads));
			break;
		case 1:
		       imx_iomux_v3_setup_multiple_pads(
			       usdhc4_pads, ARRAY_SIZE(usdhc4_pads));
		       break;
		default:
		       printf("Warning: you configured more USDHC controllers"
			       "(%d) then supported by the board (%d)\n",
			       idx + 1, CONFIG_SYS_FSL_USDHC_NUM);
		       return status;
		}

		status |= fsl_esdhc_initialize(bis, &usdhc_cfg[idx]);
	}

	return status;
}
#endif

iomux_v3_cfg_t const ecspi_pads[] = {
	/* SPI1 */
	MX6_PAD_DISP0_DAT22__ECSPI1_MISO | MUX_PAD_CTRL(SPI_PAD_CTRL),
	MX6_PAD_DISP0_DAT21__ECSPI1_MOSI | MUX_PAD_CTRL(SPI_PAD_CTRL),
	MX6_PAD_DISP0_DAT20__ECSPI1_SCLK | MUX_PAD_CTRL(SPI_PAD_CTRL),
	MX6_PAD_DISP0_DAT23__ECSPI1_SS0  | MUX_PAD_CTRL(SPI_PAD_CTRL),
	MX6_PAD_DISP0_DAT15__ECSPI1_SS1  | MUX_PAD_CTRL(SPI_PAD_CTRL),
	/* SPI2, NOR Flash nWP, CS0 */
	MX6_PAD_CSI0_DAT10__ECSPI2_MISO	| MUX_PAD_CTRL(SPI_PAD_CTRL),
	MX6_PAD_CSI0_DAT9__ECSPI2_MOSI	| MUX_PAD_CTRL(SPI_PAD_CTRL),
	MX6_PAD_CSI0_DAT8__ECSPI2_SCLK	| MUX_PAD_CTRL(SPI_PAD_CTRL),
	MX6_PAD_CSI0_DAT11__GPIO5_IO29	| MUX_PAD_CTRL(NO_PAD_CTRL),
	/*MX6_PAD_CSI0_DAT11__ECSPI2_SS0	| MUX_PAD_CTRL(SPI_PAD_CTRL),*/
	MX6_PAD_SD3_DAT5__GPIO7_IO00	| MUX_PAD_CTRL(NO_PAD_CTRL),
	/* SPI3, */
	MX6_PAD_DISP0_DAT2__ECSPI3_MISO	| MUX_PAD_CTRL(SPI_PAD_CTRL),
	MX6_PAD_DISP0_DAT1__ECSPI3_MOSI	| MUX_PAD_CTRL(SPI_PAD_CTRL),
	MX6_PAD_DISP0_DAT0__ECSPI3_SCLK	| MUX_PAD_CTRL(SPI_PAD_CTRL),
	MX6_PAD_DISP0_DAT3__ECSPI3_SS0	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_DISP0_DAT4__ECSPI3_SS1	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_DISP0_DAT5__ECSPI3_SS2	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_DISP0_DAT6__ECSPI3_SS3	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6_PAD_DISP0_DAT7__ECSPI3_RDY	| MUX_PAD_CTRL(NO_PAD_CTRL),
};

void setup_spi(void)
{
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;

	gpio_direction_output(IMX_GPIO_NR(5, 29), 1);
	gpio_direction_output(IMX_GPIO_NR(7, 0), 1);
	imx_iomux_v3_setup_multiple_pads(ecspi_pads,
					 ARRAY_SIZE(ecspi_pads));
	/* enable ECSPIx clock gating */
	setbits_le32(&mxc_ccm->CCGR1,	MXC_CCM_CCGR1_ECSPI1S_MASK |
					MXC_CCM_CCGR1_ECSPI2S_MASK |
					MXC_CCM_CCGR1_ECSPI3S_MASK);
}

#define ENET_PAD_CTRL_CLK  ((PAD_CTL_PUS_100K_UP & ~PAD_CTL_PKE) | \
	PAD_CTL_SPEED_HIGH | PAD_CTL_DSE_40ohm | PAD_CTL_SRE_FAST)

#ifdef CONFIG_FEC_MXC
iomux_v3_cfg_t const enet_pads[] = {
	MX6_PAD_ENET_TXD1__ENET_1588_EVENT0_IN	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_RXD1__ENET_1588_EVENT3_OUT	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_MDIO__ENET_MDIO		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_MDC__ENET_MDC		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_REF_CLK__ENET_TX_CLK	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	/*MX6_PAD_ENET_REF_CLK__ENET_TX_CLK	| MUX_PAD_CTRL(ENET_PAD_CTRL_CLK),*/

	/* for old evalboard with R159 present and R160 not populated */
	MX6_PAD_GPIO_16__ENET_REF_CLK		| MUX_PAD_CTRL(NO_PAD_CTRL),

	MX6_PAD_RGMII_TXC__RGMII_TXC		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_TD0__RGMII_TD0		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_TD1__RGMII_TD1		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_TD2__RGMII_TD2		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_TD3__RGMII_TD3		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_TX_CTL__RGMII_TX_CTL	| MUX_PAD_CTRL(ENET_PAD_CTRL),

	/* temp. for 125 MHz clock messurement on the switch's P6_INDV(100) pin */
	/*MX6_PAD_RGMII_TX_CTL__ENET_REF_CLK	| MUX_PAD_CTRL(ENET_PAD_CTRL),*/

	MX6_PAD_RGMII_RXC__RGMII_RXC		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_RD0__RGMII_RD0		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_RD1__RGMII_RD1		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_RD2__RGMII_RD2		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_RD3__RGMII_RD3		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_RX_CTL__RGMII_RX_CTL	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	/*INT#_GBE*/
	MX6_PAD_ENET_TX_EN__GPIO1_IO28		| MUX_PAD_CTRL(NO_PAD_CTRL),
};

static void setup_iomux_enet(void)
{
	imx_iomux_v3_setup_multiple_pads(enet_pads, ARRAY_SIZE(enet_pads));
	gpio_direction_input(IMX_GPIO_NR(1, 28)); /*INT#_GBE*/
}

int switch_fec_phy_read(struct mii_dev *bus, int phyAddr, int dev_addr, int regAddr);
int switch_fec_phy_write(struct mii_dev *bus, int phyAddr, int dev_addr, int regAddr, u16 data);

struct mii_dev *sw_get_miibus(uint32_t base_addr, int dev_id)
{
	struct mii_dev *bus;
	int ret;

	bus = mdio_alloc();
	if (!bus) {
		printf("mdio_alloc failed\n");
		return NULL;
	}
	bus->read = switch_fec_phy_read;
	bus->write = switch_fec_phy_write;
	bus->priv = NULL;
	sprintf(bus->name, "SW");

	ret = mdio_register(bus);
	if (ret) {
		printf("mdio_register failed\n");
		free(bus);
		return NULL;
	}
	return bus;
}

int switch_init(void)
{
	int ret;
	u16 val = 0;

	ret = miiphy_read ("FEC", 0x16, 0, &val);
	if (!ret && ((val & 0xf) == 0x7)) {
		ret = miiphy_write ("FEC", 0x16, 1, 0x13);
		if (ret) {
			printf("PHY config failed %d\n", ret);
			return -1;
		}
		udelay(1);
		ret = miiphy_write ("FEC", 0x16, 1, 0xc0fe);
		if (ret) {
			printf("PHY config failed %d\n", ret);
			return -1;
		}
	}
	ret = miiphy_read ("FEC", 0x15, 0, &val);
	if (!ret && ((val & 0xf) == 0x7)) {
		ret = miiphy_write ("FEC", 0x15, 1, 0x13);
		if (ret) {
			printf("PHY config failed %d\n", ret);
			return -1;
		}
		udelay(1);
		ret = miiphy_write ("FEC", 0x15, 1, 0xc0fe);
		if (ret) {
			printf("PHY config failed %d\n", ret);
			return -1;
		}
	}
	return 0;
}

int board_eth_init(bd_t *bis)
{
	uint32_t base = IMX_FEC_BASE;
	struct mii_dev *bus = NULL;
	struct phy_device *phydev = NULL;
	struct mii_dev *sw_bus = NULL;
	int ret;
	u16 val = 0;
	u32 reg;

	setup_iomux_enet();

	__raw_writel(0x000c0000, 0x020e0790);
	__raw_writel(0x00002003, 0x020c80e0);
        udelay(1);
	/* enet refclk sel out */
	reg = __raw_readl(0x020e0004);
	reg |= (1 << 21);
	__raw_writel(reg, 0x020e0004);

	bus = fec_get_miibus(base, -1);
	if (!bus)
		return 0;

	ret = miiphy_read ("FEC", 0x16, 0, &val);
	if (val != 0xffff) {
		switch_init();

		sw_bus = sw_get_miibus(0, 0);
		if (!sw_bus) {
			printf("no switch mii bus registered\n");
			return 0;
		}
	}

	phydev = create_fixed_phy(bus, 0xffffffff, 0, PHY_INTERFACE_MODE_RGMII);
	if (!phydev) {
		if (sw_bus)
			free(sw_bus);
		return 0;
	}
	debug("using fixed phy %d\n", phydev->addr);

	ret  = fec_probe(bis, -1, base, bus, phydev);
	if (ret) {
		printf("FEC MXC: %s:failed\n", __func__);
		free(phydev);
		free(bus);
		if (sw_bus)
			free(sw_bus);
	}
	return 0;
}
#endif

iomux_v3_cfg_t const usb_pads[] = {
	MX6_PAD_EIM_D30__USB_H1_OC | MUX_PAD_CTRL(NO_PAD_CTRL),
	/* Host Power enable */
	MX6_PAD_EIM_D31__GPIO3_IO31 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static iomux_v3_cfg_t const otg_pads[] = {
	MX6_PAD_GPIO_1__USB_OTG_ID              | MUX_PAD_CTRL(WEAK_PULLUP),
	MX6_PAD_KEY_COL4__USB_OTG_OC            | MUX_PAD_CTRL(WEAK_PULLUP),
	MX6_PAD_EIM_D30__USB_H1_OC              | MUX_PAD_CTRL(WEAK_PULLUP),
	/* OTG Power enable */
	MX6_PAD_EIM_D22__GPIO3_IO22             | MUX_PAD_CTRL(OUTPUT_40OHM),
};

#define GP_USB_OTG_PWR  IMX_GPIO_NR(3, 22)
#define GP_USB_H1_PWR  IMX_GPIO_NR(3, 31)

#ifdef CONFIG_USB_EHCI_MX6
int board_ehci_hcd_init(int port)
{
	unsigned int gpio;

	debug("USB init, port %d\n", port);

	if (port == 0) {
		imx_iomux_v3_setup_multiple_pads(otg_pads, ARRAY_SIZE(otg_pads));
		gpio = GP_USB_OTG_PWR;
	}
	if (port == 1) {
		imx_iomux_v3_setup_multiple_pads(usb_pads, ARRAY_SIZE(usb_pads));
		gpio = GP_USB_H1_PWR;
	}
	gpio_direction_output(gpio, 0); /* power on */
	mdelay(2);
	return 0;
}

int board_ehci_power(int port, int on)
{
	unsigned int gpio;

	debug("USB PWR, port %d: %d\n", port, on);

	if (port == 0)
		gpio = GP_USB_OTG_PWR;
	if (port == 1)
		gpio = GP_USB_H1_PWR;

	gpio_set_value(gpio, !on);
	return 0;
}
#endif

#if defined(CONFIG_VIDEO_IPUV3)

#define LCD0_BL_ON	IMX_GPIO_NR(5, 7)
#define LCD0_BL_EN	IMX_GPIO_NR(5, 13)
#define LCD0_VCC_EN	IMX_GPIO_NR(7, 6)
/*
#define LCD1_BL_ON	IMX_GPIO_NR(x, y)
#define LCD1_BL_EN	IMX_GPIO_NR(x, y)
#define LCD1_VCC_EN	IMX_GPIO_NR(x, y)
*/

static iomux_v3_cfg_t const backlight_pads[] = {

	/* Backlight on LVDS connector: X47 and X49 */
	/* LVDS0 */
	MX6_PAD_DISP0_DAT19__GPIO5_IO13	| MUX_PAD_CTRL(NO_PAD_CTRL), /* LCD0 BL PWR */
	MX6_PAD_DISP0_DAT13__GPIO5_IO07 | MUX_PAD_CTRL(NO_PAD_CTRL), /* LCD0 BL ON */
	MX6_PAD_SD3_DAT2__GPIO7_IO06	| MUX_PAD_CTRL(NO_PAD_CTRL), /* LCD0 VCC_EN */
	/* LVDS1 */
	/*TODO*/
};

struct display_info_t {
	int	bus;
	int	addr;
	int	pixfmt;
	int	(*detect)(struct display_info_t const *dev);
	void	(*enable)(struct display_info_t const *dev);
	struct	fb_videomode mode;
};


static int detect_hdmi(struct display_info_t const *dev)
{
	struct hdmi_regs *hdmi	= (struct hdmi_regs *)HDMI_ARB_BASE_ADDR;
	return readb(&hdmi->phy_stat0) & HDMI_DVI_STAT;
}

static void do_enable_hdmi(struct display_info_t const *dev)
{
	imx_enable_hdmi_phy();
}

static int detect_i2c(struct display_info_t const *dev)
{
	return ((0 == i2c_set_bus_num(dev->bus))
		&&
		(0 == i2c_probe(dev->addr)));
}

static void enable_lvds0(struct display_info_t const *dev)
{
	struct iomuxc *iomux = (struct iomuxc *)IOMUXC_BASE_ADDR;
	struct pwm_regs *pwm = (struct pwm_regs*)PWM2_BASE_ADDR;
	u32 reg;

	debug("lcd0 en\n");
	reg = readl(&iomux->gpr[2]);
	reg |= IOMUXC_GPR2_DATA_WIDTH_CH0_24BIT;
	writel(reg, &iomux->gpr[2]);

	gpio_direction_output(LCD0_VCC_EN, 1);
	gpio_direction_output(LCD0_BL_EN, 1);
	mdelay(200);
	gpio_direction_output(LCD0_BL_ON, 1);
	mdelay(10);

	/* Enable backlight PWM with 90% duty cycle */
	reg = 0x01c20050;
	writel(reg, &pwm->cr);
	writel(0x0000c15c, &pwm->sar);
	writel(0x0000d6d6, &pwm->pr);
	reg |= 1;
	writel(reg, &pwm->cr);
}

static void enable_lvds1(struct display_info_t const *dev)
{
	debug("lcd1 en\n");
}

static struct display_info_t const displays[] = {{
	.bus	= -1,
	.addr	= 0,
	.pixfmt	= IPU_PIX_FMT_RGB24,
	.detect	= detect_hdmi,
	.enable	= do_enable_hdmi,
	.mode	= {
		.name           = "HDMI",
		.refresh        = 60,
		.xres           = 1024,
		.yres           = 768,
		.pixclock       = 15385,
		.left_margin    = 220,
		.right_margin   = 40,
		.upper_margin   = 21,
		.lower_margin   = 7,
		.hsync_len      = 60,
		.vsync_len      = 10,
		.sync           = FB_SYNC_EXT,
		.vmode          = FB_VMODE_NONINTERLACED
} }, {
	.bus	= 2,
	.addr	= 0x4,
	.pixfmt	= IPU_PIX_FMT_LVDS666,
	.detect	= detect_i2c,
	.enable	= enable_lvds0,
	.mode	= {
		.name           = "LVDS0",
		.refresh        = 60,
		.xres           = 800,
		.yres           = 600,
		.pixclock       = 25000,
		.left_margin    = 120,
		.right_margin   = 80,
		.upper_margin   = 11,
		.lower_margin   = 7,
		.hsync_len      = 56,
		.vsync_len      = 10,
		.sync           = FB_SYNC_EXT,
		.vmode          = FB_VMODE_NONINTERLACED
} }, {
	.bus	= 2,
	.addr	= 0x38,
	.pixfmt	= IPU_PIX_FMT_LVDS666,
	.detect	= detect_i2c,
	.enable	= enable_lvds1,
	.mode	= {
		.name           = "LVDS1",
		.refresh        = 60,
		.xres           = 800,
		.yres           = 600,
		.pixclock       = 25000,
		.left_margin    = 120,
		.right_margin   = 80,
		.upper_margin   = 11,
		.lower_margin   = 7,
		.hsync_len      = 56,
		.vsync_len      = 10,
		.sync           = FB_SYNC_EXT,
		.vmode          = FB_VMODE_NONINTERLACED
} }
};

int board_video_skip(void)
{
	int i;
	int ret;
	char const *panel = getenv("panel");
	if (!panel) {
		for (i = 0; i < ARRAY_SIZE(displays); i++) {
			struct display_info_t const *dev = displays+i;
			if (dev->detect(dev)) {
				panel = dev->mode.name;
				printf("auto-detected panel %s\n", panel);
				break;
			}
		}
		if (!panel) {
			panel = displays[0].mode.name;
			printf("No panel detected: default to %s\n", panel);
			i = 0;
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(displays); i++) {
			if (!strcmp(panel, displays[i].mode.name))
				break;
		}
	}
	if (i < ARRAY_SIZE(displays)) {
		ret = ipuv3_fb_init(&displays[i].mode, 0,
				    displays[i].pixfmt);
		if (!ret) {
			displays[i].enable(displays+i);
			printf("Display: %s (%ux%u)\n",
			       displays[i].mode.name,
			       displays[i].mode.xres,
			       displays[i].mode.yres);
		} else {
			printf("LCD %s cannot be configured: %d\n",
			       displays[i].mode.name, ret);
		}
	} else {
		printf("unsupported panel %s\n", panel);
		ret = -EINVAL;
	}
	return (0 != ret);
}

static void setup_display(void)
{
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	struct iomuxc *iomux = (struct iomuxc *)IOMUXC_BASE_ADDR;
	int reg;

	if (enable_video_pll(46, 0xa2c2a, 0xf4240)) {
		printf("Can't enable PLL5.\n");
		return;
	}

	enable_ipu_clock();
	imx_setup_hdmi();
	/* Turn on LDB0,IPU,IPU DI0 clocks */
	reg = __raw_readl(&mxc_ccm->CCGR3);
	reg |=  MXC_CCM_CCGR3_LDB_DI0_MASK;
	writel(reg, &mxc_ccm->CCGR3);

	/* set LDB0, LDB1 clk select to 000/000 (PLL5 src) */
	reg = readl(&mxc_ccm->cs2cdr);
	reg &= ~(MXC_CCM_CS2CDR_LDB_DI0_CLK_SEL_MASK
		 |MXC_CCM_CS2CDR_LDB_DI1_CLK_SEL_MASK);
	writel(reg, &mxc_ccm->cs2cdr);

	reg = readl(&mxc_ccm->cscmr2);
	reg |= MXC_CCM_CSCMR2_LDB_DI0_IPU_DIV;
	writel(reg, &mxc_ccm->cscmr2);

	reg = readl(&mxc_ccm->chsccdr);
	reg |= (CHSCCDR_CLK_SEL_LDB_DI0
		<<MXC_CCM_CHSCCDR_IPU1_DI0_CLK_SEL_OFFSET);
	writel(reg, &mxc_ccm->chsccdr);

	reg = IOMUXC_GPR2_BGREF_RRMODE_EXTERNAL_RES
	     |IOMUXC_GPR2_DI1_VS_POLARITY_ACTIVE_HIGH
	     |IOMUXC_GPR2_DI0_VS_POLARITY_ACTIVE_LOW
	     |IOMUXC_GPR2_BIT_MAPPING_CH1_SPWG
	     |IOMUXC_GPR2_DATA_WIDTH_CH1_18BIT
	     |IOMUXC_GPR2_BIT_MAPPING_CH0_SPWG
	     |IOMUXC_GPR2_DATA_WIDTH_CH0_18BIT
	     |IOMUXC_GPR2_LVDS_CH1_MODE_DISABLED
	     |IOMUXC_GPR2_LVDS_CH0_MODE_ENABLED_DI0;
	writel(reg, &iomux->gpr[2]);

	reg = readl(&iomux->gpr[3]);
	reg = (reg & ~(IOMUXC_GPR3_LVDS0_MUX_CTL_MASK
			|IOMUXC_GPR3_HDMI_MUX_CTL_MASK))
	    | (IOMUXC_GPR3_MUX_SRC_IPU1_DI0
	       <<IOMUXC_GPR3_LVDS0_MUX_CTL_OFFSET);
	writel(reg, &iomux->gpr[3]);

	imx_iomux_v3_setup_multiple_pads(backlight_pads,
					 ARRAY_SIZE(backlight_pads));
	/* backlights off until needed */
	gpio_direction_input(LCD0_VCC_EN);
	gpio_direction_input(LCD0_BL_EN);
	gpio_direction_input(LCD0_BL_ON);
	/*
	gpio_direction_input(LCD1_VCC_EN);
	gpio_direction_input(LCD1_BL_EN);
	gpio_direction_input(LCD1_BL_ON);
	*/
}
#endif

int board_early_init_f(void)
{
	setup_iomux_uart();

	gpio_direction_output(GP_USB_H1_PWR, 1); /* host power off */
	gpio_direction_output(GP_USB_OTG_PWR, 1); /* OTG power off */
#if defined(CONFIG_VIDEO_IPUV3)
	setup_display();
#endif
	return 0;
}

/*
 * Do not overwrite the console
 * Use always serial for U-Boot console
 */
int overwrite_console(void)
{
	return 1;
}

#if defined(CONFIG_OF_LIBFDT) && defined(CONFIG_OF_BOARD_SETUP)
void ft_board_setup(void *blob, bd_t *bd)
{
	fdt_fixup_ethernet(blob);
}
#endif

/* LEDs
static iomux_v3_cfg_t const led_pads[] = {
};
 */

int board_init(void)
{
	struct iomuxc_base_regs *const iomuxc_regs
		= (struct iomuxc_base_regs *)IOMUXC_BASE_ADDR;

	clrsetbits_le32(&iomuxc_regs->gpr[1],
			IOMUXC_GPR1_OTG_ID_MASK,
			IOMUXC_GPR1_OTG_ID_GPIO1);

	/* address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM + 0x100;

	setup_spi();

	setup_i2c(0, CONFIG_SYS_I2C_SPEED, 0x7f, &i2c_pad_info0);
	setup_i2c(1, CONFIG_SYS_I2C_SPEED, 0x7f, &i2c_pad_info1);
	setup_i2c(2, CONFIG_SYS_I2C_SPEED, 0x7f, &i2c_pad_info2);

	imx_iomux_v3_setup_multiple_pads(misc_pads, ARRAY_SIZE(misc_pads));

	/*imx_iomux_v3_setup_multiple_pads(led_pads, ARRAY_SIZE(led_pads));*/
	/*#if defined(CONFIG_STATUS_LED) && defined(STATUS_LED_BOOT)*/
	/*status_led_set(STATUS_LED_BOOT, STATUS_LED_STATE);*/
	/*#endif*/
	return 0;
}

int power_init_board(void)
{
/*
	u32 ret;
	struct pmic *p;

	ret = pmic_init(I2C_PMIC);
	if (ret)
		return ret;

	p = pmic_get("FSL_PMIC");
	if (!p)
		return -1;

	pf0100_setpwrstage(p, sw2pwrstg, 80);
	pf0100_setpwrstage(p, sw3bpwrstg, 60);
	pf0100_setpwrstage(p, sw4pwrstg, 40);
	pf0100_setvoltage(p, sw2volt, 2500);
	pf0100_setvoltage(p, sw1aboff, 1000);
	pf0100_setvoltage(p, sw1coff, 1050);
	pf0100_setvoltage(p, sw3aoff, 400);
	pf0100_setvoltage(p, sw3boff, 400);
	pf0100_setvoltage(p, sw4off, 400);
	pmic_reg_write(p, offsetof(struct pf0100, sw1abconf),
		SWxDVSSPEED_32US | SWxFREQ_2_MHZ | SWxABILIM);
	pmic_reg_write(p, offsetof(struct pf0100, sw1cconf),
		SWxDVSSPEED_32US | SWxPHASE_90| SWxFREQ_2_MHZ | SWxABILIM);
	pmic_reg_write(p, offsetof(struct pf0100, sw2conf),
		SWxDVSSPEED_32US | SWxPHASE_270| SWxFREQ_2_MHZ | SWxABILIM);
	pmic_reg_write(p, offsetof(struct pf0100, sw3aconf),
		SWxDVSSPEED_32US | SWxPHASE_180| SWxFREQ_2_MHZ | SWxABILIM);
	pmic_reg_write(p, offsetof(struct pf0100, sw3bconf),
		SWxDVSSPEED_32US | SWxPHASE_180| SWxFREQ_2_MHZ | SWxABILIM);
	pmic_reg_write(p, offsetof(struct pf0100, sw4conf),
		SWxDVSSPEED_32US | SWxPHASE_270| SWxFREQ_2_MHZ | SWxABILIM);
*/
	return 0;
}

int checkboard(void)
{
	puts("Board: legcom\n");

	return 0;
}

int board_phy_config(struct phy_device *phydev)
{
	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}


#ifdef CONFIG_CMD_BMODE
static const struct boot_mode board_boot_modes[] = {
	/* 4 bit bus width */
	{"mmc0",	MAKE_CFGVAL(0x40, 0x30, 0x00, 0x00)},
	{NULL,		0},
};
#endif

int misc_init_r(void)
{
#ifdef CONFIG_CMD_BMODE
	add_board_boot_modes(board_boot_modes);
#endif
#ifdef CONFIG_HW_WATCHDOG
	hw_watchdog_init();
#endif
	return 0;
}

#ifdef CONFIG_POST
int arch_memory_test_advance(u32 *vstart, u32 *size, phys_addr_t *phys_offset)
{
	debug("advance POST: start 0x%lx, size 0x%lx, offs 0x%lx\n",
	      (ulong)*vstart, (ulong)*size, (ulong)*phys_offset);

	/* running from OCRAM, stop since already tested the whole range */
	if (CONFIG_SYS_TEXT_BASE < CONFIG_SYS_SDRAM_BASE)
		return 1;

	if (*vstart < CONFIG_SYS_TEXT_BASE) {
		/*
		 * previous step tested the area above monitor image,
		 * now test the area below the image
		 */
		*vstart = CONFIG_SYS_TEXT_BASE + gd->mon_len;
		*size = PHYS_SDRAM_SIZE - (CONFIG_SYS_TEXT_BASE -
					   CONFIG_SYS_SDRAM_BASE + gd->mon_len);
		return 0;
	}
	return 1;
}

int arch_memory_test_prepare(u32 *vstart, u32 *size, phys_addr_t *phys_offset)
{
	if (CONFIG_SYS_TEXT_BASE < CONFIG_SYS_SDRAM_BASE) {
		/* started from OCRAM, can test all DRAM */
		*vstart = CONFIG_SYS_SDRAM_BASE;
		*size = PHYS_SDRAM_SIZE;
	} else {
		/* skip 4k at the start of DRAM */
		if (CONFIG_SYS_SDRAM_BASE + 0x1000 < CONFIG_SYS_TEXT_BASE) {
			*vstart = CONFIG_SYS_SDRAM_BASE + 0x1000;
			*size = CONFIG_SYS_TEXT_BASE - CONFIG_SYS_SDRAM_BASE -
				0x1000;
		} else {
			*vstart = CONFIG_SYS_TEXT_BASE + gd->mon_len;

			*size = PHYS_SDRAM_SIZE -
				(CONFIG_SYS_TEXT_BASE - CONFIG_SYS_SDRAM_BASE +
				 gd->mon_len);
		}
	}
	debug("Memory POST: start 0x%lx, size 0x%lx\n",
	      (ulong)*vstart, (ulong)*size);
	return 0;
}
#endif
