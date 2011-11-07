/*
 * (C) Copyright 2011
 * DENX Software Engineering, Anatolij Gustschin <agust@denx.de>
 *
 * Derived from P1022DS board config
 * Copyright 2010-2011 Freescale Semiconductor, Inc.
 * Authors: Srikanth Srinivasan <srikanth.srinivasan@freescale.com>
 *          Timur Tabi <timur@freescale.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#ifdef CONFIG_36BIT
#define CONFIG_PHYS_64BIT
#endif

/* High Level Configuration Options */
#define CONFIG_BOOKE			/* BOOKE */
#define CONFIG_E500			/* BOOKE e500 family */
#define CONFIG_MPC85xx			/* MPC8540/60/55/41/48 */
#define CONFIG_P1022
#define CONFIG_EBX200
#define CONFIG_MP			/* support multiple processors */

#ifndef CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_TEXT_BASE	0xeff80000
#endif

#ifndef CONFIG_RESET_VECTOR_ADDRESS
#define CONFIG_RESET_VECTOR_ADDRESS	0xeffffffc
#endif

#define CONFIG_FSL_ELBC			/* Has Enhanced localbus controller */

#ifdef CONFIG_PHYS_64BIT
#define CONFIG_ENABLE_36BIT_PHYS
#define CONFIG_ADDR_MAP
#define CONFIG_SYS_NUM_ADDR_MAP		16	/* number of TLB1 entries */
#endif

#define CONFIG_FSL_LAW			/* Use common FSL init code */

#define CONFIG_SYS_CLK_FREQ	66000000
#define CONFIG_DDR_CLK_FREQ	66000000

/*
 * These can be toggled for performance analysis, otherwise use default.
 */
#define CONFIG_L2_CACHE
#define CONFIG_BTB

#define CONFIG_SYS_MEMTEST_START	0x00000000
#define CONFIG_SYS_MEMTEST_END		0x7fffffff

/*
 * Base addresses -- Note these are effective addresses where the
 * actual resources get mapped (not physical addresses)
 */
#define CONFIG_SYS_CCSRBAR_DEFAULT	0xff700000	/* CCSRBAR Default */
#define CONFIG_SYS_CCSRBAR		0xffe00000	/* relocated CCSRBAR */
#ifdef CONFIG_PHYS_64BIT
#define CONFIG_SYS_CCSRBAR_PHYS		0xfffe00000ull
#else
#define CONFIG_SYS_CCSRBAR_PHYS		CONFIG_SYS_CCSRBAR
#endif
#define CONFIG_SYS_IMMR			CONFIG_SYS_CCSRBAR

#define CONFIG_LAST_STAGE_INIT

/*
 * DDR Setup
 */
#define CONFIG_SYS_DDR_SDRAM_BASE	0x00000000
#define CONFIG_SYS_SDRAM_BASE		CONFIG_SYS_DDR_SDRAM_BASE
#define CONFIG_SYS_SDRAM_SIZE		(512 * 1024 * 1024ul)

#define CONFIG_NUM_DDR_CONTROLLERS	1
#define CONFIG_CHIP_SELECTS_PER_CTRL	1

#define CONFIG_SYS_DDR_CS0_BNDS		0x0000001F
#define CONFIG_SYS_DDR_CS1_BNDS		0x00000000
#define CONFIG_SYS_DDR_CS0_CONFIG	0x80014102
#define CONFIG_SYS_DDR_CS1_CONFIG	0x00000000

#define CONFIG_SYS_DDR_TIMING_3		0x01041000
#define CONFIG_SYS_DDR_TIMING_0		0x00440104
#define CONFIG_SYS_DDR_TIMING_1		0x98912c56
#define CONFIG_SYS_DDR_TIMING_2		0x0fb8c8de

#define CONFIG_SYS_DDR_MODE_1		0x40040c51
#define CONFIG_SYS_DDR_MODE_2		0x8050c000
#define CONFIG_SYS_DDR_MODE_CTRL	0x00000000
#define CONFIG_SYS_DDR_INTERVAL		0x0a280100
#define CONFIG_SYS_DDR_DATA_INIT	0xdeadbeef
#define CONFIG_SYS_DDR_CLK_CTRL		0x02000000
#define CONFIG_SYS_DDR_TIMING_4		0x00220001
#define CONFIG_SYS_DDR_TIMING_5		0x00002400
#define CONFIG_SYS_DDR_ZQ_CNTL		0x89080600
#define CONFIG_SYS_DDR_WRLVL_CNTL	0x00000000
#define CONFIG_SYS_DDR_CONTROL		0x47200008 /* Type = DDR3: No Interleaving */
#define CONFIG_SYS_DDR_CONTROL2		0x44401050
#define CONFIG_SYS_DDR_CDR_1		0x80000000
#define CONFIG_SYS_DDR_CDR_2		0x00000000

#define CONFIG_SYS_DDR_ERR_INT_EN	0x00000000
#define CONFIG_SYS_DDR_ERR_DIS		0x00000000
#define CONFIG_SYS_DDR_SBE		0x00000000

/*
 * Memory map
 *
 * 0x0000_0000	0x1fff_ffff	DDR			512M Cacheable
 * 0x8000_0000	0xdfff_ffff	PCI Express Mem		1.5G non-cacheable
 * 0xffc0_0000	0xffc2_ffff	PCI IO range		192K non-cacheable
 *
 * Localbus cacheable (TBD)
 * 0xXXXX_XXXX	0xXXXX_XXXX	SRAM			YZ M Cacheable
 *
 * Localbus non-cacheable
 * 0xe800_0000	0xefff_ffff	FLASH			128M non-cacheable
 * 0xffd0_0000	0xffd0_3fff	L1 for stack		16K Cacheable TLB0
 * 0xffe0_0000	0xffef_ffff	CCSR			1M non-cacheable
 */

/*
 * Local Bus Definitions
 */
#define CONFIG_SYS_FLASH_BASE		0xe8000000 /* start of FLASH 128M */
#ifdef CONFIG_PHYS_64BIT
#define CONFIG_SYS_FLASH_BASE_PHYS	0xfe8000000ull
#else
#define CONFIG_SYS_FLASH_BASE_PHYS	CONFIG_SYS_FLASH_BASE
#endif

#define CONFIG_FLASH_BR_PRELIM  \
	(BR_PHYS_ADDR(CONFIG_SYS_FLASH_BASE_PHYS) | BR_PS_16 | BR_V)
#define CONFIG_FLASH_OR_PRELIM	(OR_AM_128MB | 0xf46)

#define CONFIG_SYS_BR0_PRELIM	CONFIG_FLASH_BR_PRELIM  /* NOR Base Address */
#define CONFIG_SYS_OR0_PRELIM	CONFIG_FLASH_OR_PRELIM  /* NOR Options */

#define CONFIG_SYS_FLASH_BANKS_LIST	\
	{(CONFIG_SYS_FLASH_BASE_PHYS)}
#define CONFIG_FLASH_SHOW_PROGRESS	45 /* count down from 45/5: 9..1 */

#define CONFIG_SYS_MAX_FLASH_BANKS	1
#define CONFIG_SYS_MAX_FLASH_SECT	1024

#define CONFIG_SYS_MONITOR_BASE		CONFIG_SYS_TEXT_BASE	/* start of monitor */

#define CONFIG_FLASH_CFI_DRIVER
#define CONFIG_SYS_FLASH_CFI
#define CONFIG_SYS_FLASH_EMPTY_INFO
#define CONFIG_SYS_FLASH_PROTECTION	1

/*
 * Init
 */
#define CONFIG_BOARD_EARLY_INIT_F
#define CONFIG_BOARD_EARLY_INIT_R
#define CONFIG_MISC_INIT_R
#define CONFIG_HWCONFIG

#define CONFIG_SYS_INIT_RAM_LOCK
#define CONFIG_SYS_INIT_RAM_ADDR	0xffd00000 /* Initial L1 address */
#define CONFIG_SYS_INIT_RAM_SIZE	0x00004000 /* Size of used area in RAM */

#define CONFIG_SYS_GBL_DATA_OFFSET	\
	(CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_OFFSET	CONFIG_SYS_GBL_DATA_OFFSET

#define CONFIG_SYS_MONITOR_LEN		(512 * 1024)
#define CONFIG_SYS_MALLOC_LEN		(6 * 1024 * 1024)

/*
 * Serial Port
 */
#define CONFIG_CONS_INDEX		2
#define CONFIG_SYS_NS16550
#define CONFIG_SYS_NS16550_SERIAL
#define CONFIG_SYS_NS16550_REG_SIZE	1
#define CONFIG_SYS_NS16550_CLK		get_bus_freq(0)

#define CONFIG_SYS_BAUDRATE_TABLE	\
	{300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200}

#define CONFIG_SYS_NS16550_COM1	(CONFIG_SYS_CCSRBAR + 0x4500)
#define CONFIG_SYS_NS16550_COM2	(CONFIG_SYS_CCSRBAR + 0x4600)

/* Use the HUSH parser */
#define CONFIG_SYS_HUSH_PARSER
#define CONFIG_SYS_PROMPT_HUSH_PS2 "> "

/*
 * Pass open firmware flat tree
 */
#define CONFIG_OF_LIBFDT
#define CONFIG_OF_BOARD_SETUP
#define CONFIG_OF_STDOUT_VIA_ALIAS

/* new uImage format support */
#define CONFIG_FIT
#define CONFIG_FIT_VERBOSE

#ifdef CONFIG_PCI
#define CONFIG_PCI_PNP			/* do pci plug-and-play */
#define CONFIG_PCI_SCAN_SHOW		/* show pci devices on startup */
#endif

/*
 * MMC
 */
#define CONFIG_MMC
#ifdef CONFIG_MMC
#define CONFIG_CMD_MMC
#define CONFIG_FSL_ESDHC
#define CONFIG_GENERIC_MMC
#define CONFIG_SYS_FSL_ESDHC_ADDR	CONFIG_SYS_MPC85xx_ESDHC_ADDR
#define CONFIG_CMD_EXT2
#define CONFIG_CMD_FAT
#define CONFIG_DOS_PARTITION
#endif

/*
 * Network
 */
#define CONFIG_TSEC_ENET
#ifdef CONFIG_TSEC_ENET

#define CONFIG_TSECV2
#define CONFIG_NET_MULTI

#define CONFIG_MII		/* MII PHY management */
#define CONFIG_TSEC1		1
#define CONFIG_TSEC1_NAME	"eTSEC1"
#define CONFIG_TSEC2		1
#define CONFIG_TSEC2_NAME	"eTSEC2"

#define TSEC1_PHY_ADDR		0x1C
#define TSEC2_PHY_ADDR		9

#define TSEC1_FLAGS		(TSEC_GIGABIT | TSEC_REDUCED)
#define TSEC2_FLAGS		(TSEC_GIGABIT)

#define TSEC1_PHYIDX		0
#define TSEC2_PHYIDX		0

#define CONFIG_ETHPRIME		"eTSEC1"

#define CONFIG_PHY_GIGE		/* Include GbE speed/duplex detection */

/*
 * In-band SGMII auto-negotiation between TBI and Marvell PHY, force
 * 1000mbps SGMII link
 */
#define CONFIG_TSEC_TBICR_SETTINGS ( \
		TBICR_PHY_RESET \
		| TBICR_ANEG_ENABLE \
		| TBICR_FULL_DUPLEX \
		| TBICR_SPEED1_SET \
		)

#define CONFIG_SYS_TBIPA_VALUE	0x10
#endif

/*
 * Environment
 */
#define CONFIG_ENV_IS_IN_FLASH
#define CONFIG_ENV_OVERWRITE
#define CONFIG_ENV_SIZE		0x2000
#define CONFIG_ENV_SECT_SIZE	0x20000
#define CONFIG_ENV_ADDR		(CONFIG_SYS_MONITOR_BASE - CONFIG_ENV_SECT_SIZE)

#define CONFIG_LOADS_ECHO
#define CONFIG_SYS_LOADS_BAUD_CHANGE

/*
 * Command line configuration.
 */
#include <config_cmd_default.h>

#define CONFIG_CMD_ELF
#define CONFIG_CMD_ERRATA
#define CONFIG_CMD_IRQ
#define CONFIG_CMD_MII
#define CONFIG_CMD_NET
#define CONFIG_CMD_PING
#define CONFIG_CMD_SETEXPR
#define CONFIG_CMD_REGINFO

#ifdef CONFIG_PCI
#define CONFIG_CMD_PCI
#endif

/*
 * Miscellaneous configurable options
 */
#define CONFIG_SYS_LONGHELP			/* undef to save memory	*/
#define CONFIG_CMDLINE_EDITING			/* Command-line editing */
#define CONFIG_AUTO_COMPLETE			/* add autocompletion support */
#define CONFIG_SYS_LOAD_ADDR	0x2000000	/* default load address */
#define CONFIG_SYS_PROMPT	"=> "		/* Monitor Command Prompt */
#ifdef CONFIG_CMD_KGDB
#define CONFIG_SYS_CBSIZE	1024		/* Console I/O Buffer Size */
#else
#define CONFIG_SYS_CBSIZE	256		/* Console I/O Buffer Size */
#endif
/* Print Buffer Size */
#define CONFIG_SYS_PBSIZE (CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT) + 16)
#define CONFIG_SYS_MAXARGS	16
#define CONFIG_SYS_BARGSIZE	CONFIG_SYS_CBSIZE
#define CONFIG_SYS_HZ		1000

/*
 * For booting Linux, the board info and command line data
 * have to be in the first 64 MB of memory, since this is
 * the maximum mapped by the Linux kernel during initialization.
 */
#define CONFIG_SYS_BOOTMAPSZ	(64 << 20)	/* Initial Memory map for Linux*/
#define CONFIG_SYS_BOOTM_LEN	(64 << 20)	/* Increase max gunzip size */

#ifdef CONFIG_CMD_KGDB
#define CONFIG_KGDB_BAUDRATE	230400	/* speed to run kgdb serial port */
#define CONFIG_KGDB_SER_INDEX	2	/* which serial port to use */
#endif

/*
 * Environment Configuration
 */
#define CONFIG_HOSTNAME		ebx200
#define CONFIG_ROOTPATH		/opt/eldk/ppc_85xxDP
#define CONFIG_BOOTFILE		ebx200/uImage
#define CONFIG_UBOOTPATH	ebx200/u-boot.bin

#define CONFIG_LOADADDR		1000000

#define CONFIG_BOOTDELAY	10	/* -1 disables auto-boot */
#define CONFIG_BOOTARGS

#define CONFIG_BAUDRATE		115200

#define	CONFIG_EXTRA_ENV_SETTINGS					\
	"netdev=eth0\0"							\
	"uboot=" MK_STR(CONFIG_UBOOTPATH) "\0"				\
	"tftpflash=tftpboot $loadaddr $uboot; "				\
		"protect off " MK_STR(CONFIG_SYS_TEXT_BASE) " +$filesize; "	\
		"erase " MK_STR(CONFIG_SYS_TEXT_BASE) " +$filesize; "		\
		"cp.b $loadaddr " MK_STR(CONFIG_SYS_TEXT_BASE) " $filesize; "	\
		"protect on " MK_STR(CONFIG_SYS_TEXT_BASE) " +$filesize; "	\
		"cmp.b $loadaddr " MK_STR(CONFIG_SYS_TEXT_BASE) " $filesize\0"	\
	"consoledev=ttyS1\0"						\
	"ramdiskaddr=2000000\0"						\
	"ramdiskfile=uramdisk\0"  		      	        	\
	"fdtaddr=c00000\0"	  			      		\
	"fdtfile=ebx200/ebx200.dtb\0"  					\
	"hwconfig=esdhc\0"

#define CONFIG_NFSBOOTCOMMAND						\
	"setenv bootargs root=/dev/nfs rw "				\
	"nfsroot=$serverip:$rootpath "					\
	"ip=$ipaddr:$serverip:$gatewayip:$netmask:$hostname:$netdev:off " \
	"console=$consoledev,$baudrate $othbootargs;"			\
	"tftp $loadaddr $bootfile;"					\
	"tftp $fdtaddr $fdtfile;"					\
	"bootm $loadaddr - $fdtaddr"

#define CONFIG_RAMBOOTCOMMAND						\
	"setenv bootargs root=/dev/ram rw "				\
	"console=$consoledev,$baudrate $othbootargs;"			\
	"tftp $ramdiskaddr $ramdiskfile;"				\
	"tftp $loadaddr $bootfile;"					\
	"tftp $fdtaddr $fdtfile;"					\
	"bootm $loadaddr $ramdiskaddr $fdtaddr"

#define CONFIG_BOOTCOMMAND		CONFIG_NFSBOOTCOMMAND

#endif
