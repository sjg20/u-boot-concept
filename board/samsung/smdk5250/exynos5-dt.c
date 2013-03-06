/*
 * Copyright (C) 2012 Samsung Electronics
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
#include <cros_ec.h>
#include <fdtdec.h>
#include <asm/io.h>
#include <errno.h>
#include <i2c.h>
#include <netdev.h>
#include <spi.h>
#include <asm/arch/cpu.h>
#include <asm/arch/dwmmc.h>
#include <asm/arch/gpio.h>
#include <asm/arch/mmc.h>
#include <asm/arch/pinmux.h>
#include <asm/arch/power.h>
#include <asm/arch/sromc.h>
#include <power/pmic.h>
#include <power/max77686_pmic.h>

DECLARE_GLOBAL_DATA_PTR;

struct local_info {
	struct cros_ec_dev *cros_ec_dev;	/* Pointer to cros_ec device */
	int cros_ec_err;			/* Error for cros_ec, 0 if ok */
	int arbitrate_node;
	struct fdt_gpio_state ap_claim;
	struct fdt_gpio_state ec_claim;

	/* Time between requesting bus and deciding that we have it */
	unsigned slew_delay_us;

	/* Time between retrying to see if the AP has released the bus */
	unsigned wait_retry_ms;

	/* Time to wait until the bus becomes free */
	unsigned wait_free_ms;
};

static struct local_info local;

#ifdef CONFIG_USB_EHCI_EXYNOS
int board_usb_vbus_init(void)
{
	struct exynos5_gpio_part1 *gpio1 = (struct exynos5_gpio_part1 *)
						samsung_get_base_gpio_part1();

	/* Enable VBUS power switch */
	s5p_gpio_direction_output(&gpio1->x2, 6, 1);

	/* VBUS turn ON time */
	mdelay(3);

	return 0;
}
#endif

struct cros_ec_dev *board_get_cros_ec_dev(void)
{
	return local.cros_ec_dev;
}

static int board_init_cros_ec_devices(const void *blob)
{
	local.cros_ec_err = cros_ec_init(blob, &local.cros_ec_dev);
	if (local.cros_ec_err)
		return -1;  /* Will report in board_late_init() */

	return 0;
}

static int board_i2c_arb_init(const void *blob)
{
	int node;

	local.arbitrate_node = -1;
	node = fdtdec_next_compatible(blob, 0, COMPAT_GOOGLE_ARBITRATOR);
	node = -1; /* disable until GPIOs are working */
	if (node < 0) {
		debug("Cannot find bus arbitrator node\n");
		return 0;
	}

	if (fdtdec_decode_gpio(blob, node, "google,ap-claim-gpios",
				&local.ap_claim) ||
			fdtdec_decode_gpio(blob, node, "google,ec-claim-gpios",
				&local.ec_claim)) {
		debug("Cannot find bus arbitrator GPIOs\n");
		return 0;
	}

	if (fdtdec_setup_gpio(&local.ap_claim) ||
			fdtdec_setup_gpio(&local.ec_claim)) {
		debug("Cannot claim arbitration GPIOs\n");
		return -1;
	}

	/* We are currently not claiming the bus */
	gpio_direction_output(local.ap_claim.gpio, 1);
	gpio_direction_input(local.ec_claim.gpio);

	local.arbitrate_node = fdtdec_lookup_phandle(blob, node,
						     "google,arbitrate-bus");
	if (local.arbitrate_node < 0) {
		debug("Cannot find bus to arbitrate\n");
		return -1;
	}

	local.slew_delay_us = fdtdec_get_int(blob, node,
					     "google,slew-delay-us", 10);
	local.wait_retry_ms = fdtdec_get_int(blob, node,
					     "google,wait-retry-us", 2000);
	local.wait_free_ms = fdtdec_get_int(blob, node,
					    "google,wait-free-us", 50000);
	local.wait_free_ms = DIV_ROUND_UP(local.wait_free_ms, 1000);
	debug("Bus arbitration ready on fdt node %d\n", local.arbitrate_node);

	return 0;
}

int board_init(void)
{
	gd->bd->bi_boot_params = (PHYS_SDRAM_1 + 0x100UL);
#ifdef CONFIG_EXYNOS_SPI
	spi_init();
#endif

	if (board_init_cros_ec_devices(gd->fdt_blob))
		return -1;

	if (board_i2c_arb_init(gd->fdt_blob))
		return -1;

#ifdef CONFIG_USB_EHCI_EXYNOS
	board_usb_vbus_init();
#endif
	return 0;
}

int dram_init(void)
{
	int i;
	u32 addr;

	for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++) {
		addr = CONFIG_SYS_SDRAM_BASE + (i * SDRAM_BANK_SIZE);
		gd->ram_size += get_ram_size((long *)addr, SDRAM_BANK_SIZE);
	}
	return 0;
}

#if defined(CONFIG_POWER)
static int pmic_reg_update(struct pmic *p, int reg, uint regval)
{
	u32 val;
	int ret = 0;

	ret = pmic_reg_read(p, reg, &val);
	if (ret) {
		debug("%s: PMIC %d register read failed\n", __func__, reg);
		return -1;
	}
	val |= regval;
	ret = pmic_reg_write(p, reg, val);
	if (ret) {
		debug("%s: PMIC %d register write failed\n", __func__, reg);
		return -1;
	}
	return 0;
}

int power_init_board(void)
{
	struct pmic *p;

	set_ps_hold_ctrl();

	i2c_init(CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);

	if (pmic_init(I2C_PMIC))
		return -1;

	p = pmic_get("MAX77686_PMIC");
	if (!p)
		return -ENODEV;

	if (pmic_probe(p))
		return -1;

	if (pmic_reg_update(p, MAX77686_REG_PMIC_32KHZ, MAX77686_32KHCP_EN))
		return -1;

	if (pmic_reg_update(p, MAX77686_REG_PMIC_BBAT,
				MAX77686_BBCHOSTEN | MAX77686_BBCVS_3_5V))
		return -1;

	/* VDD_MIF */
	if (pmic_reg_write(p, MAX77686_REG_PMIC_BUCK1OUT,
						MAX77686_BUCK1OUT_1V)) {
		debug("%s: PMIC %d register write failed\n", __func__,
						MAX77686_REG_PMIC_BUCK1OUT);
		return -1;
	}

	if (pmic_reg_update(p, MAX77686_REG_PMIC_BUCK1CRTL,
						MAX77686_BUCK1CTRL_EN))
		return -1;

	/* VDD_ARM */
	if (pmic_reg_write(p, MAX77686_REG_PMIC_BUCK2DVS1,
					MAX77686_BUCK2DVS1_1_3V)) {
		debug("%s: PMIC %d register write failed\n", __func__,
						MAX77686_REG_PMIC_BUCK2DVS1);
		return -1;
	}

	if (pmic_reg_update(p, MAX77686_REG_PMIC_BUCK2CTRL1,
					MAX77686_BUCK2CTRL_ON))
		return -1;

	/* VDD_INT */
	if (pmic_reg_write(p, MAX77686_REG_PMIC_BUCK3DVS1,
					MAX77686_BUCK3DVS1_1_0125V)) {
		debug("%s: PMIC %d register write failed\n", __func__,
						MAX77686_REG_PMIC_BUCK3DVS1);
		return -1;
	}

	if (pmic_reg_update(p, MAX77686_REG_PMIC_BUCK3CTRL,
					MAX77686_BUCK3CTRL_ON))
		return -1;

	/* VDD_G3D */
	if (pmic_reg_write(p, MAX77686_REG_PMIC_BUCK4DVS1,
					MAX77686_BUCK4DVS1_1_2V)) {
		debug("%s: PMIC %d register write failed\n", __func__,
						MAX77686_REG_PMIC_BUCK4DVS1);
		return -1;
	}

	if (pmic_reg_update(p, MAX77686_REG_PMIC_BUCK4CTRL1,
					MAX77686_BUCK3CTRL_ON))
		return -1;

	/* VDD_LDO2 */
	if (pmic_reg_update(p, MAX77686_REG_PMIC_LDO2CTRL1,
				MAX77686_LD02CTRL1_1_5V | EN_LDO))
		return -1;

	/* VDD_LDO3 */
	if (pmic_reg_update(p, MAX77686_REG_PMIC_LDO3CTRL1,
				MAX77686_LD03CTRL1_1_8V | EN_LDO))
		return -1;

	/* VDD_LDO5 */
	if (pmic_reg_update(p, MAX77686_REG_PMIC_LDO5CTRL1,
				MAX77686_LD05CTRL1_1_8V | EN_LDO))
		return -1;

	/* VDD_LDO10 */
	if (pmic_reg_update(p, MAX77686_REG_PMIC_LDO10CTRL1,
				MAX77686_LD10CTRL1_1_8V | EN_LDO))
		return -1;

	return 0;
}
#endif

void dram_init_banksize(void)
{
	int i;
	u32 addr, size;

	for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++) {
		addr = CONFIG_SYS_SDRAM_BASE + (i * SDRAM_BANK_SIZE);
		size = get_ram_size((long *)addr, SDRAM_BANK_SIZE);

		gd->bd->bi_dram[i].start = addr;
		gd->bd->bi_dram[i].size = size;
	}
}

/**
 * Read and clear the marker value; then return the read value.
 *
 * This marker is set to EXYNOS5_SPL_MARKER when SPL runs. Then in U-Boot
 * we can check (and clear) this marker to see if we were run from SPL.
 * If we were called from another U-Boot, the marker will be clear.
 *
 * @return marker value (EXYNOS5_SPL_MARKER if we were run from SPL, else 0)
 */
static uint32_t exynos5_read_and_clear_spl_marker(void)
{
	uint32_t value, *marker = (uint32_t *)CONFIG_SPL_MARKER;

	value = *marker;
	*marker = 0;

	return value;
}

int board_is_processor_reset(void)
{
	static uint8_t inited, is_reset;
	uint32_t marker_value;

	if (!inited) {
		marker_value = exynos5_read_and_clear_spl_marker();
		is_reset = marker_value == EXYNOS5_SPL_MARKER;
		inited = 1;
	}

	return is_reset;
}
static int decode_sromc(const void *blob, struct fdt_sromc *config)
{
	int err;
	int node;

	node = fdtdec_next_compatible(blob, 0, COMPAT_SAMSUNG_EXYNOS5_SROMC);
	if (node < 0) {
		debug("Could not find SROMC node\n");
		return node;
	}

	config->bank = fdtdec_get_int(blob, node, "bank", 0);
	config->width = fdtdec_get_int(blob, node, "width", 2);

	err = fdtdec_get_int_array(blob, node, "srom-timing", config->timing,
			FDT_SROM_TIMING_COUNT);
	if (err < 0) {
		debug("Could not decode SROMC configuration"
					"Error: %s\n", fdt_strerror(err));
		return -FDT_ERR_NOTFOUND;
	}
	return 0;
}

int board_eth_init(bd_t *bis)
{
#ifdef CONFIG_SMC911X
	u32 smc_bw_conf, smc_bc_conf;
	struct fdt_sromc config;
	fdt_addr_t base_addr;
	int node;

	node = decode_sromc(gd->fdt_blob, &config);
	if (node < 0) {
		debug("%s: Could not find sromc configuration\n", __func__);
		return 0;
	}
	node = fdtdec_next_compatible(gd->fdt_blob, node, COMPAT_SMSC_LAN9215);
	if (node < 0) {
		debug("%s: Could not find lan9215 configuration\n", __func__);
		return 0;
	}

	/* We now have a node, so any problems from now on are errors */
	base_addr = fdtdec_get_addr(gd->fdt_blob, node, "reg");
	if (base_addr == FDT_ADDR_T_NONE) {
		debug("%s: Could not find lan9215 address\n", __func__);
		return -1;
	}

	/* Ethernet needs data bus width of 16 bits */
	if (config.width != 2) {
		debug("%s: Unsupported bus width %d\n", __func__,
			config.width);
		return -1;
	}
	smc_bw_conf = SROMC_DATA16_WIDTH(config.bank)
			| SROMC_BYTE_ENABLE(config.bank);

	smc_bc_conf = SROMC_BC_TACS(config.timing[FDT_SROM_TACS])   |
			SROMC_BC_TCOS(config.timing[FDT_SROM_TCOS]) |
			SROMC_BC_TACC(config.timing[FDT_SROM_TACC]) |
			SROMC_BC_TCOH(config.timing[FDT_SROM_TCOH]) |
			SROMC_BC_TAH(config.timing[FDT_SROM_TAH])   |
			SROMC_BC_TACP(config.timing[FDT_SROM_TACP]) |
			SROMC_BC_PMC(config.timing[FDT_SROM_PMC]);

	/* Select and configure the SROMC bank */
	exynos_pinmux_config(PERIPH_ID_SROMC, config.bank);
	s5p_config_sromc(config.bank, smc_bw_conf, smc_bc_conf);
	return smc911x_initialize(0, base_addr);
#endif
	return 0;
}

#ifdef CONFIG_DISPLAY_BOARDINFO
int checkboard(void)
{
#ifdef CONFIG_OF_CONTROL
	const char *board_name;

	board_name = fdt_getprop(gd->fdt_blob, 0, "model", NULL);
	printf("\nBoard: %s, rev %d\n", board_name ? board_name : "<unknown>",
	       board_get_revision());
#else
	printf("\nBoard: SMDK5250\n");
#endif

	return 0;
}
#endif

#ifdef CONFIG_GENERIC_MMC
int board_mmc_init(bd_t *bis)
{
	int ret;
	/* dwmmc initializattion for available channels */
	ret = exynos_dwmmc_init(gd->fdt_blob);
	if (ret)
		debug("dwmmc init failed\n");

	return ret;
}
#endif

static int board_uart_init(void)
{
	int err, uart_id, ret = 0;

	for (uart_id = PERIPH_ID_UART0; uart_id <= PERIPH_ID_UART3; uart_id++) {
		err = exynos_pinmux_config(uart_id, PINMUX_FLAG_NONE);
		if (err) {
			debug("UART%d not configured\n",
					 (uart_id - PERIPH_ID_UART0));
			ret |= err;
		}
	}
	return ret;
}

#ifdef CONFIG_BOARD_EARLY_INIT_F
int board_early_init_f(void)
{
	int err;
	err = board_uart_init();
	if (err) {
		debug("UART init failed\n");
		return err;
	}
#ifdef CONFIG_SYS_I2C_INIT_BOARD
	board_i2c_init(gd->fdt_blob);
#endif
	return err;
}
#endif

int board_get_revision(void)
{
	struct fdt_gpio_state gpios[CONFIG_BOARD_REV_GPIO_COUNT];
	unsigned gpio_list[CONFIG_BOARD_REV_GPIO_COUNT];
	int board_rev = -1;
	int count = 0;
	int node;

	node = fdtdec_next_compatible(gd->fdt_blob, 0,
				      COMPAT_GOOGLE_BOARD_REV);
	if (node >= 0) {
		count = fdtdec_decode_gpios(gd->fdt_blob, node,
				"google,board-rev-gpios", gpios,
				CONFIG_BOARD_REV_GPIO_COUNT);
	}
	if (count > 0) {
		int i;

		for (i = 0; i < count; i++)
			gpio_list[i] = gpios[i].gpio;
		board_rev = gpio_decode_number(gpio_list, count);
	} else {
		debug("%s: No board revision information in fdt\n", __func__);
	}

	return board_rev;
}

void board_i2c_release_bus(int node)
{
	/* If this is us, release the bus */
	if (node == local.arbitrate_node) {
		gpio_set_value(local.ap_claim.gpio, 1);
		udelay(local.slew_delay_us);
	}
}

int board_i2c_claim_bus(int node)
{
	unsigned start;

	if (node != local.arbitrate_node)
		return 0;

// 	putc('c');

	/* Start a round of trying to claim the bus */
	start = get_timer(0);
	do {
		unsigned start_retry;
		int waiting = 0;

		/* Indicate that we want to claim the bus */
		gpio_set_value(local.ap_claim.gpio, 0);
		udelay(local.slew_delay_us);

		/* Wait for the EC to release it */
		start_retry = get_timer(0);
		while (get_timer(start_retry) < local.wait_retry_ms) {
			if (gpio_get_value(local.ec_claim.gpio)) {
				/* We got it, so return */
				return 0;
			}

			if (!waiting) {
				waiting = 1;
			}
		}

		/* It didn't release, so give up, wait, and try again */
		gpio_set_value(local.ap_claim.gpio, 1);

		mdelay(local.wait_retry_ms);
	} while (get_timer(start) < local.wait_free_ms);

	/* Give up, release our claim */
	printf("I2C: Could not claim bus, timeout %lu\n", get_timer(start));

	return -1;
}

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
	stdio_print_current_devices();

	if (local.cros_ec_err) {
		/* Force console on */
		gd->flags &= ~GD_FLG_SILENT;

		printf("cros-ec communications failure %d\n", local.cros_ec_err);
		puts("\nPlease reset with Power+Refresh\n\n");
		panic("Cannot init cros-ec device");
		return -1;
	}
	return 0;
}
#endif
