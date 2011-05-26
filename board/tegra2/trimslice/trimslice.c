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
#include <nand.h>
#include <netdev.h>
#include <spi.h>
#include <asm/io.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-types.h>
#include <asm/arch/nvcommon.h>
#include <asm/arch/nv_hardware_access.h>
#include <asm/arch/nv_drf.h>
#include <asm/arch/tegra2.h>
#include <asm/arch/gpio.h>
#include <asm/arch/nvboot_bit.h>
#include <asm/arch/nvbl_memmap_nvap.h>
#include "../common/board.h"
#include "../common/pinmux.h"

#include "trimslice.h"

void board_spi_init(void)
{
	spi_init();
	/* Pull-up SPI1_MISO line - will be used for software shutdown monitoring*/
	tg2_gpio_direction_output(23, 7, 1);
}

int is_mmc_boot()
{
  NvBootDevType dev_type = ((NvBootInfoTable*)(AP20_BASE_PA_BOOT_INFO))->SecondaryDevice;
  if (dev_type == NvBootDevType_Sdmmc)
    return 1;
  return 0;
}
/***************************************************************************
 * Routines for SD/EMMC board specific configuration.
 ***************************************************************************/

/***************************************************************************
 * Routines for UART board specific configuration.
 ***************************************************************************/
void NvBlUartClockInitA(void)
{
    NvU32 Reg;

    // 1. Assert Reset to UART A
    NV_CLK_RST_READ(RST_DEVICES_L, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L,
                             SWR_UARTA_RST, ENABLE, Reg);
    NV_CLK_RST_WRITE(RST_DEVICES_L, Reg);

    // 2. Enable clk to UART A
    NV_CLK_RST_READ(CLK_OUT_ENB_L, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L,
                             CLK_ENB_UARTA, ENABLE, Reg);
    NV_CLK_RST_WRITE(CLK_OUT_ENB_L, Reg);


    // Override pllp setup for 216MHz operation.
    Reg = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BYPASS, ENABLE)
          | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_ENABLE, DISABLE)
          | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_REF_DIS, REF_ENABLE)
          | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BASE_OVRRIDE, ENABLE)
          | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_LOCK, 0x0)
          | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVP, 0x1)
          | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVN,
                       NVRM_PLLP_FIXED_FREQ_KHZ/500)
          | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVM, 0x0C);
    NV_CLK_RST_WRITE(PLLP_BASE, Reg);

    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE,
                             PLLP_ENABLE, ENABLE, Reg);
    NV_CLK_RST_WRITE(PLLP_BASE, Reg);

    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE,
                             PLLP_BYPASS, DISABLE, Reg);
    NV_CLK_RST_WRITE(PLLP_BASE, Reg);

    // Enable pllp_out0 to UARTA.
    Reg = NV_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_UARTA,
                     UARTA_CLK_SRC, PLLP_OUT0);
    NV_CLK_RST_WRITE(CLK_SOURCE_UARTA, Reg);


    // wait for 2us
    NvBlAvpStallUs(2);

    // De-assert reset to UART A
    NV_CLK_RST_READ(RST_DEVICES_L, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L,
                             SWR_UARTA_RST, DISABLE, Reg);
    NV_CLK_RST_WRITE(RST_DEVICES_L, Reg);

}

static struct tegra_pingroup_config trimslice_early_uarta_pinmux[] = {
	{TEGRA_PINGROUP_DAP2,  TEGRA_MUX_DAP2,          TEGRA_PUPD_NORMAL,    TEGRA_TRI_NORMAL}, /* Normal */
	{TEGRA_PINGROUP_GPU,   TEGRA_MUX_UARTA,           TEGRA_PUPD_NORMAL,    TEGRA_TRI_NORMAL},
	{TEGRA_PINGROUP_UAC,   TEGRA_MUX_RSVD,         TEGRA_PUPD_NORMAL,    TEGRA_TRI_NORMAL},
	{TEGRA_PINGROUP_IRRX,  TEGRA_MUX_UARTB,         TEGRA_PUPD_NORMAL,   TEGRA_TRI_TRISTATE},
	{TEGRA_PINGROUP_IRTX,  TEGRA_MUX_UARTB,         TEGRA_PUPD_NORMAL,   TEGRA_TRI_TRISTATE},
	{TEGRA_PINGROUP_SDB,   TEGRA_MUX_SPI2,           TEGRA_PUPD_NORMAL,    TEGRA_TRI_TRISTATE},
	{TEGRA_PINGROUP_SDD,   TEGRA_MUX_SPI3,           TEGRA_PUPD_NORMAL,   TEGRA_TRI_TRISTATE},
	{TEGRA_PINGROUP_SDIO1, TEGRA_MUX_SDIO1,         TEGRA_PUPD_NORMAL,    TEGRA_TRI_TRISTATE},
};

void
NvBlUartInitA(void)
{
    NvU32 Reg;

    NvBlUartClockInitA();

    /* /\* Enable UARTA - Harmony board uses config4 *\/ */
    /* CONFIG(A,D,GPU,UARTA); */
    tegra_pinmux_config_table(trimslice_early_uarta_pinmux,
			      ARRAY_SIZE(trimslice_early_uarta_pinmux));

    // Prepare the divisor value.
    Reg = NVRM_PLLP_FIXED_FREQ_KHZ * 1000 / NV_DEFAULT_DEBUG_BAUD / 16;

    // Set up UART parameters.
    NV_UARTA_WRITE(LCR,        0x80);
    NV_UARTA_WRITE(THR_DLAB_0, Reg);
    NV_UARTA_WRITE(IER_DLAB_0, 0x00);
    NV_UARTA_WRITE(LCR,        0x00);
    NV_UARTA_WRITE(IIR_FCR,    0x37);
    NV_UARTA_WRITE(IER_DLAB_0, 0x00);
    NV_UARTA_WRITE(LCR,        0x03);  // 8N1
    NV_UARTA_WRITE(MCR,        0x02);
    NV_UARTA_WRITE(MSR,        0x00);
    NV_UARTA_WRITE(SPR,        0x00);
    NV_UARTA_WRITE(IRDA_CSR,   0x00);
    NV_UARTA_WRITE(ASR,        0x00);

    NV_UARTA_WRITE(IIR_FCR,    0x31);

    // Flush any old characters out of the RX FIFO.
    while (NvBlUartRxReadyA())
        (void)NvBlUartRxA();
}

void
NvBlUartInit(void)
{
#if (CONFIG_TEGRA2_ENABLE_UARTA)
    NvBlUartInitA();
#endif
}

int board_late_init(void)
{
  return 0;
}

int board_eth_init(bd_t *bis)
{
	return pci_eth_init(bis);
}

extern int tegra_pcie_init(int init_port0, int init_port1);
void pci_init_board(void)
{
	tegra_pcie_init(1, 0);
}

int drv_keyboard_init(void)
{
	return drv_usb_kbd_init();
}

void usb1_set_host_mode(void)
{
	tg2_gpio_direction_output(21, 2, 1);
	tg2_gpio_direction_output(21, 3, 0);
}

#define mdelay(n) ({unsigned long msec=(n); while (msec--) udelay(1000);})
void board_sata_reset(void)
{
	/*printf("performing SATA reset ...\n");*/

	tg2_gpio_direction_output(0,3,0);
	mdelay(500);
	tg2_gpio_direction_output(0,3,1);
	mdelay(3000);
}
