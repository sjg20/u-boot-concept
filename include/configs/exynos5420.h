/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * Common settings for Exynos5420 boards.
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

#ifndef __CONFIG_EXYNOS5420_H
#define __CONFIG_EXYNOS5420_H

#define CONFIG_EXYNOS5420
#define CONFIG_VAR_SIZE_SPL

#define CONFIG_SYS_SDRAM_BASE		0x20000000
#ifdef CONFIG_CROS_RO
#define CONFIG_SYS_TEXT_BASE		0x02030000
#else
#define CONFIG_SYS_TEXT_BASE		0x23E00000
#endif
#define CONFIG_SPL_TEXT_START		0x02024400
#define CONFIG_SPL_TLB_BASE		0x0202c000

#ifdef CONFIG_VAR_SIZE_SPL
/* There is a 16-byte header */
#define CONFIG_SPL_TEXT_BASE		0x02024410
#define CONFIG_SPL_HEADER_SIZE		16
#else
#define CONFIG_SPL_TEXT_BASE		CONFIG_SPL_TEXT_START
#define CONFIG_SPL_HEADER_SIZE		0
#endif
#define CONFIG_IRAM_TOP			0x02074000

#define CONFIG_DEVICE_TREE_LIST "exynos5420-peach-pit" \
	" exynos5420-peach-pit-rev1 exynos5420-smdk5420"

/* DRAM Memory Banks */
#define CONFIG_NR_DRAM_BANKS	7
#define SDRAM_BANK_SIZE		(512UL << 20)	/* 512 MB */

/* Multiple USB controller support */
#define CONFIG_USB_MAX_CONTROLLER_COUNT		2

#define CONFIG_LOADADDR			23E00000

#endif
