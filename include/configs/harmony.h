/*
 *  (C) Copyright 2010,2011
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

#ifndef __CONFIG_H
#define __CONFIG_H

#include <asm/sizes.h>
#include "tegra2-common.h"

/* High-level configuration options */
#define TEGRA2_SYSMEM		"mem=384M@0M nvmem=128M@384M mem=512M@512M"
#define V_PROMPT		"Tegra2 (Harmony) # "
#define CONFIG_TEGRA2_BOARD_STRING	"NVIDIA Harmony"

/* Board-specific serial config */
#define CONFIG_SERIAL_MULTI

#ifndef CONFIG_OF_CONTROL

#define CONFIG_TEGRA2_ENABLE_UARTD

/* UARTD: keyboard satellite board UART, default */
#define CONFIG_SYS_NS16550_COM1		NV_PA_APB_UARTD_BASE
#ifdef CONFIG_TEGRA2_ENABLE_UARTA
/* UARTA: debug board UART */
#define CONFIG_SYS_NS16550_COM2		NV_PA_APB_UARTA_BASE
#endif

/* To select the order in which U-Boot sees USB ports */
#define CONFIG_TEGRA2_USB0      NV_PA_USB3_BASE
#define CONFIG_TEGRA2_USB1      NV_PA_USB1_BASE
#define CONFIG_TEGRA2_USB2      0
#define CONFIG_TEGRA2_USB3      0

/* Put USB1 in host mode */
#define CONFIG_TEGRA2_USB1_HOST
#endif /* CONFIG_OF_CONTROL */


#define CONFIG_MACH_TYPE		MACH_TYPE_HARMONY
#define CONFIG_SYS_BOARD_ODMDATA	0x300d8011 /* lp1, 1GB */

/* Environment not stored */
#define CONFIG_ENV_IS_NOWHERE

#define CONFIG_TEGRA2_GPIO
#define CONFIG_CMD_TEGRA2_GPIO_INFO

#define CONFIG_DEFAULT_DEVICE_TREE "tegra2-harmony"

/* NAND support */
#define CONFIG_CMD_NAND
#define CONFIG_TEGRA2_NAND

/*
 * For HYNIX HY27UF4G2B NAND device.
 * Get the following timing values from data sheet.
 */

/*
 * non-EDO mode: value (in ns) = Max(tRP, tREA) + 6ns
 * EDO mode: value (in ns) = tRP timing
 */
#define CONFIG_NAND_MAX_TRP_TREA	26

#define CONFIG_NAND_TWB		100

/* Value = Max(tCR, tAR, tRR) */
#define CONFIG_NAND_MAX_TCR_TAR_TRR	20
#define CONFIG_NAND_TWHR		80

/* Value = Max(tCS, tCH, tALS, tALH) */
#define CONFIG_NAND_MAX_TCS_TCH_TALS_TALH	20
#define CONFIG_NAND_TWH			10
#define CONFIG_NAND_TWP			12
#define CONFIG_NAND_TRH			10
#define CONFIG_NAND_TADL		70

/* How many bytes for data area */
#define CONFIG_NAND_PAGE_DATA_BYTES	2048

/*
 * How many bytes in spare area
 * spare area = skipped bytes + ECC bytes of data area
 * + tag bytes + ECC bytes of tag bytes
 */
#define CONFIG_NAND_PAGE_SPARE_BYTES	64

#define CONFIG_NAND_SKIPPED_SPARE_BYTES	4

/* How many ECC bytes for data area */
#define CONFIG_NAND_RS_DATA_ECC_BYTES	36

/* How many tag bytes in spare area */
#define CONFIG_NAND_TAG_BYTES		20

/* How many ECC bytes to be generated for tag bytes */
#define CONFIG_NAND_TAG_ECC_BYTES	4

/* n bits */
#define CONFIG_NAND_BUS_WIDTH	8

/* GPIO port C bit 7, C.07, GMI_WP->NAND_WP */
#define CONFIG_NAND_WP_GPIO	GPIO_PC7

/* physical address to access nand at CS0 */
#define CONFIG_SYS_NAND_BASE	TEGRA2_NAND_BASE

/* Max number of NAND devices */
#define CONFIG_SYS_MAX_NAND_DEVICE	1
#endif /* __CONFIG_H */
