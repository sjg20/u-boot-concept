/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Board configuration file for Qualcomm SDM845 MTP board
 *
 * Copyright 2018 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __CONFIGS_SDM845_MTP_H
#define __CONFIGS_SDM845_MTP_H

#include <linux/sizes.h>

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN		SZ_1M

#define CONFIG_ENV_SIZE			SZ_8K

#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_SDRAM_BASE + 0x7fff0)
#define CONFIG_SYS_LOAD_ADDR		(CONFIG_SYS_SDRAM_BASE + 0x80000)

/* Monitor Command Prompt */
#define CONFIG_SYS_CBSIZE		512
#define CONFIG_SYS_MAXARGS		64

#define PHYS_SDRAM_1			0x80000000
#define PHYS_SDRAM_1_SIZE		0x60000000

#define CONFIG_SYS_SDRAM_BASE		PHYS_SDRAM_1

#define CONFIG_SYS_BOOTM_LEN		SZ_64M

#endif
