/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2022 Google LLC
 */
#ifndef __SOCFGPA_CHAMELEONV3_H__
#define __SOCFGPA_CHAMELEONV3_H__

#include <asm/arch/base_addr_a10.h>

#define CONFIG_SYS_BOOTM_LEN	(32 * 1024 * 1024)

/*
 * U-Boot general configurations
 */

/* Memory configurations  */
#define PHYS_SDRAM_1_SIZE		0x40000000

/*
 * Serial / UART configurations
 */
#define CONFIG_SYS_NS16550_MEM32
#define CONFIG_SYS_BAUDRATE_TABLE {4800, 9600, 19200, 38400, 57600, 115200}

#define CONFIG_EXTRA_ENV_SETTINGS \
	"distro_bootcmd=bridge enable; " \
		"load mmc 0:1 ${loadaddr} u-boot.txt; " \
		"env import -t ${loadaddr}; " \
		"run bootcmd_txt\0"

/*
 * L4 OSC1 Timer 0
 */
/* reload value when timer count to zero */
#define TIMER_LOAD_VAL			0xFFFFFFFF

/* SPL memory allocation configuration, this is for FAT implementation */
#define CONFIG_SYS_SPL_MALLOC_SIZE	0x00015000

/* The rest of the configuration is shared */
#include <configs/socfpga_common.h>

#endif	/* __SOCFGPA_CHAMELEONV3_H__ */
