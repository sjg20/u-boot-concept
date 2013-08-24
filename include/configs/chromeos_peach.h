/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * Configuration settings for the SAMSUNG Peach board.
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

#ifndef __CONFIG_CHROMEOS_PEACH_H
#define __CONFIG_CHROMEOS_PEACH_H

#define CONFIG_CROS_EC
#define CONFIG_FTHREAD

#define CONFIG_RTC_MAX77802
/*
#define CONFIG_ELOG
#define CONFIG_SPL_ELOG_SUPPORT
*/
#define CONFIG_SPL_MAX77802_BUS 4
#define CONFIG_SPL_MAX77802_ADDR 9
#define CONFIG_SPL_RTC_SUPPORT
#define CONFIG_SPL_I2C_SUPPORT
#define CONFIG_ELOG_SIZE 0x4000
#define CONFIG_ELOG_OFFSET (0x00400000 - CONFIG_ENV_SIZE - CONFIG_ELOG_SIZE)

/*
 * For now we have to hard-code some additional settings for Exynos5420.
 * Eventually these should move to the FDT.
 */
#include <configs/exynos5420.h>

#include <configs/exynos5-dt.h>

#include <configs/chromeos.h>

#define CONFIG_CHROMEOS_PEACH

#undef CONFIG_DEFAULT_DEVICE_TREE
#define CONFIG_DEFAULT_DEVICE_TREE	exynos5420-peach-pit
#define CONFIG_STD_DEVICES_SETTINGS    EXYNOS_DEVICE_SETTINGS

#define CONFIG_SYS_PROMPT	"Peach # "
#define CONFIG_IDENT_STRING	" for Peach"

/* We don't use this on peach - this saves a small about of SPL space */
#undef CONFIG_EXYNOS_LPDDR3

/*
 * Extra bootargs used for direct booting, but not for vboot.
 * - console of the board
 * - debug and earlyprintk: easier to debug; they could be removed later
 */
#define CONFIG_DIRECT_BOOTARGS \
	"console=ttySAC3," STRINGIFY(CONFIG_BAUDRATE) " debug earlyprintk"

#define CONFIG_EXTRA_BOOTARGS ""

#define ANX1120_I2C_BUS		7

#ifdef CONFIG_CHROMEOS_DISPLAY
#define CONFIG_LCD
#define CONFIG_EXYNOS_FB
#define CONFIG_EXYNOS_DP
#endif
#define RPLL_MDIV		0x5E
#define RPLL_PDIV		0x2
#define RPLL_SDIV		0x4
#define LCD_BPP			LCD_COLOR16

#define CONFIG_CHROMEOS_GPIO_FLAG
#define CONFIG_CHROMEOS_CROS_EC_FLAG

#define CONFIG_EXYNOS_FAST_SPI_BOOT
#define CONFIG_SPL_VERIFY

/* Replace default CONFIG_EXTRA_ENV_SETTINGS */
#ifdef CONFIG_EXTRA_ENV_SETTINGS
#undef CONFIG_EXTRA_ENV_SETTINGS
#endif
#define CONFIG_EXTRA_ENV_SETTINGS \
	EXYNOS_DEVICE_SETTINGS \
	CONFIG_CHROMEOS_EXTRA_ENV_SETTINGS

/* Replace default CONFIG_BOOTCOMMAND */
#ifdef CONFIG_BOOTCOMMAND
#undef CONFIG_BOOTCOMMAND
#endif
#define CONFIG_BOOTCOMMAND CONFIG_NON_VERIFIED_BOOTCOMMAND
/*
trace pause; usb start; set autoload n; bootp; trace calls 30000000 100000; tftpput ${profbase} ${profoffset} 192.168.4.124:/tftpboot/postload.profile
*/

#define CONFIG_CROS_EC_SPI		/* Support CROS_EC over SPI */

#undef CONFIG_EXYNOS_LPDDR3

#endif	/* __CONFIG_CHROMEOS_PEACH_H */
