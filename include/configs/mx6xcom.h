/*
 * Copyright (C) 2014 DENX Software Engineering
 *
 * Configuration settings for the mx6xcom board.
 *
 * Based on mx6qsabrelite.h which is:
 * Copyright (C) 2010-2011 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include "mx6_common.h"

#define CONFIG_MX6
#define CONFIG_DISPLAY_CPUINFO
#define CONFIG_DISPLAY_BOARDINFO

#define CONFIG_MACH_TYPE	3769

#include <asm/arch/imx-regs.h>
#include <asm/imx-common/gpio.h>

#define CONFIG_CMDLINE_TAG
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_INITRD_TAG
#define CONFIG_REVISION_TAG

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN		(8 * 1024 * 1024)

#define CONFIG_BOARD_EARLY_INIT_F
#define CONFIG_MISC_INIT_R
#define CONFIG_MXC_GPIO

#define CONFIG_MXC_UART
#define CONFIG_MXC_UART_BASE		UART5_BASE

/* SPI NOR Flash */
#define CONFIG_CMD_SF
#ifdef CONFIG_CMD_SF
#define CONFIG_SPI_FLASH
#define CONFIG_SPI_FLASH_STMICRO
#define CONFIG_SPI_FLASH_BAR
#define CONFIG_MXC_SPI
#define CONFIG_SF_DEFAULT_BUS		1
#define CONFIG_SF_DEFAULT_CS		(0|(IMX_GPIO_NR(5, 29)<<8))
#define CONFIG_SF_DEFAULT_SPEED		25000000
#define CONFIG_SF_DEFAULT_MODE		SPI_MODE_0
#endif

/* PMIC Controller */
#if 0
#define CONFIG_POWER
#define CONFIG_POWER_I2C
#define CONFIG_POWER_FSL
#define CONFIG_PMIC_FSL_PF0100
#define CONFIG_SYS_FSL_PMIC_I2C_ADDR	0x8
#endif

/* I2C Configs */
#define CONFIG_CMD_I2C
#define CONFIG_SYS_I2C
#define	CONFIG_SYS_I2C_MXC
#define CONFIG_I2C_MULTI_BUS
#define CONFIG_SYS_I2C_SPEED		100000

/* OCOTP Configs */
#define CONFIG_CMD_IMXOTP
#ifdef CONFIG_CMD_IMXOTP
#define CONFIG_IMX_OTP
#define IMX_OTP_BASE			OCOTP_BASE_ADDR
#define IMX_OTP_ADDR_MAX		0x7F
#define IMX_OTP_DATA_ERROR_VAL		0xBADABADA
#define IMX_OTPWRITE_ENABLED
#endif

/* Ethernet */
#define CONFIG_FEC_MXC
#ifdef CONFIG_FEC_MXC
#define IMX_FEC_BASE			ENET_BASE_ADDR
#define CONFIG_FEC_XCV_TYPE		RGMII
#define CONFIG_FEC_MXC_PHYADDR		0
#define CONFIG_MII
#define CONFIG_PHYLIB
/*#define CONFIG_PHY_MARVELL*/
#define CONFIG_CMD_PING
#define CONFIG_CMD_DHCP
#define CONFIG_CMD_MII
#define CONFIG_CMD_NET
#endif

#define CONFIG_CMD_SPI

/* MMC Configs */
#define CONFIG_FSL_ESDHC
#define CONFIG_FSL_USDHC
#define CONFIG_SYS_FSL_ESDHC_ADDR	0
#define CONFIG_SYS_FSL_USDHC_NUM	2

#define CONFIG_MMC
#define CONFIG_CMD_MMC
#define CONFIG_GENERIC_MMC
#define CONFIG_BOUNCE_BUFFER
#define CONFIG_CMD_EXT2
#define CONFIG_CMD_FAT
#define CONFIG_DOS_PARTITION

/* Miscellaneous commands */
#define CONFIG_CMD_BMODE
#define CONFIG_CMD_SETEXPR
#define CONFIG_CMD_DIAG

/*
 * Status LED
#define CONFIG_CMD_LED
#define CONFIG_STATUS_LED
#define CONFIG_BOARD_SPECIFIC_LED
#define STATUS_LED_BOOT	0
#define STATUS_LED_BIT		IMX_GPIO_NR(5, 15)
#define STATUS_LED_STATE	STATUS_LED_ON
#define STATUS_LED_PERIOD	(CONFIG_SYS_HZ / 2)
#define STATUS_LED_BIT1		IMX_GPIO_NR(5, 14)
#define STATUS_LED_STATE1	STATUS_LED_ON
#define STATUS_LED_PERIOD1	(CONFIG_SYS_HZ / 2)
#define STATUS_LED_BIT2		IMX_GPIO_NR(5, 16)
#define STATUS_LED_STATE2	STATUS_LED_ON
#define STATUS_LED_PERIOD2	(CONFIG_SYS_HZ / 2)
#define STATUS_LED_BIT3		IMX_GPIO_NR(5, 17)
#define STATUS_LED_STATE3	STATUS_LED_ON
#define STATUS_LED_PERIOD3	(CONFIG_SYS_HZ / 2)
 */

/* FuSES and OTP */
#define CONFIG_CMD_FUSE
#define CONFIG_MXC_OCOTP

/* allow to overwrite serial and ethaddr */
#define CONFIG_ENV_OVERWRITE
#define CONFIG_BAUDRATE			115200

/* Command definition */
#include <config_cmd_default.h>

#undef CONFIG_CMD_IMLS

#define CONFIG_BOOTDELAY	5

#define CONFIG_PREBOOT		""

#define CONFIG_LOADADDR		0x12000000
#define CONFIG_SYS_TEXT_BASE	0x17800000

#define CONFIG_EXTRA_ENV_SETTINGS \
    "addcons=setenv bootargs ${bootargs} console=${console},${baudrate}\0" \
    "addip=setenv bootargs ${bootargs} ip=${ipaddr}:${serverip}:${gatewayip}:${netmask}:${hostname}:${netdev}:off\0" \
    "addmisc=setenv bootargs ${bootargs} ${miscargs}\0" \
    "addrecoverip=setenv bootargs ${bootargs} ip=${ipaddr}::${gatewayip}:${netmask}:recovery:${netdev}:off\0"\
    "addr_kernel=10800000\0"\
    "addr_nor_env=0x80000\0"\
    "bootdelay=5\0" \
    "ipaddr=192.168.10.54\0"\
    "gatewayip=192.168.10.1\0"\
    "netmask=255.255.255.0\0"\
    "serverip=192.168.10.30\0"\
    "netdev=eth0\0"\
    "altbootcmd=run recovery\0" \
    "baudrate=115200\0" \
    "bootcmd=run upd_uboot;setenv bootcmd echo done;saveenv\0" \
    "bootlimit=3\0" \
    "clearenv=sf probe;sf erase ${addr_nor_env} ${len_env} && echo restored environment to factory default\0" \
    "miscargs=debug\0" \
    "console=ttymxc1\0" \
    "hostname=mx6qcom\0" \
    "len_env=0x20000\0" \
    "len_recovery=0x700000\0" \
    "loadaddr=0x12000000\0" \
    "bootfile=mx6xcom/uImage\0" \
    "fdtaddr=0x12500000\0" \
    "fdtfile=mx6xcom/mx6qcom.dtb\0" \
    "nfsroot=/opt/eldk-5.3/armv7a/rootfs-qte-sdk\0" \
    "nfsargs=setenv bootargs ${bootargs} root=/dev/nfs nfsroot=${serverip}:${nfsroot},v3,tcp\0" \
    "net_nfs=tftp ${loadaddr} ${bootfile};tftp ${fdtaddr} ${fdtfile};run nfsargs addcons addip addmisc;bootm ${loadaddr} - ${fdtaddr}\0" \
    "uload=mmc rescan;fatload mmc 0:1 12000000 u-boot.imx\0" \
    "update=sf probe;sf erase 0 46000; sf write 12000000 400 ${filesize}\0" \
    "upd_uboot=if run uload;then run update;else echo stopping update...;fi\0" \
    "recoveryargs=setenv bootargs ${bootargs} root=/dev/ram0 rw eth=${ethaddr}\0"\
    "recovery=sf probe;sf read ${loadaddr} ${offset_recovery} ${len_recovery};run addrecoverip addcons recoveryargs;bootm\0"\
    "\0" \


/* Miscellaneous configurable options */
#define CONFIG_SYS_LONGHELP
#define CONFIG_SYS_HUSH_PARSER
#define CONFIG_SYS_PROMPT		"U-Boot > "
#define CONFIG_AUTO_COMPLETE
#define CONFIG_SYS_CBSIZE		1024

/* Print Buffer Size */
#define CONFIG_SYS_PBSIZE (CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT) + 16)
#define CONFIG_SYS_MAXARGS		16
#define CONFIG_SYS_BARGSIZE		CONFIG_SYS_CBSIZE

#define CONFIG_SYS_MEMTEST_START	0x10000000
#define CONFIG_SYS_MEMTEST_END		0x10010000
#define CONFIG_SYS_MEMTEST_SCRATCH	0x10800000

#define CONFIG_SYS_LOAD_ADDR		CONFIG_LOADADDR
#define CONFIG_STANDALONE_LOAD_ADDR	0x10001000
#define CONFIG_SYS_HZ			1000

#define CONFIG_CMDLINE_EDITING

/* Physical Memory Map */
#define CONFIG_NR_DRAM_BANKS		1
#define PHYS_SDRAM			MMDC0_ARB_BASE_ADDR
#define CONFIG_SYS_SDRAM_BASE		PHYS_SDRAM
#define PHYS_SDRAM_SIZE			(256u * 1024 * 1024)

#define CONFIG_SYS_INIT_RAM_ADDR	IRAM_BASE_ADDR
#define CONFIG_SYS_INIT_RAM_SIZE	IRAM_SIZE

#define CONFIG_SYS_INIT_SP_OFFSET \
	(CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR \
	(CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

/* FLASH and environment organization */
#define CONFIG_SYS_NO_FLASH

/* Commands */
/*#define CONFIG_CMD_UBI*/
/*#define CONFIG_CMD_UBIFS*/
#define CONFIG_CMD_TIME
#define CONFIG_RBTREE
#define CONFIG_LZO
#define CONFIG_MTD_PARTITIONS
#define CONFIG_MTD_DEVICE
#define CONFIG_CMD_MTDPARTS
#define CONFIG_CMD_GPIO
#define CONFIG_CMD_MEMTEST

/* ENV config */
#define CONFIG_ENV_SIZE			(8 * 1024)

#define CONFIG_ENV_IS_IN_SPI_FLASH
#define CONFIG_ENV_OFFSET		(512 * 1024)
#define CONFIG_ENV_SECT_SIZE		(8 * 1024)
#define CONFIG_SYS_REDUNDAND_ENVIRONMENT
#define CONFIG_ENV_OFFSET_REDUND	(CONFIG_ENV_OFFSET + \
						CONFIG_ENV_SECT_SIZE)
#define CONFIG_ENV_SIZE_REDUND		CONFIG_ENV_SIZE
#define CONFIG_ENV_SPI_BUS		CONFIG_SF_DEFAULT_BUS
#define CONFIG_ENV_SPI_CS		CONFIG_SF_DEFAULT_CS
#define CONFIG_ENV_SPI_MODE		CONFIG_SF_DEFAULT_MODE
#define CONFIG_ENV_SPI_MAX_HZ		CONFIG_SF_DEFAULT_SPEED

#define CONFIG_FIT
#define CONFIG_OF_LIBFDT
#define CONFIG_OF_BOARD_SETUP
#define CONFIG_CMD_BOOTZ

#ifndef CONFIG_SYS_DCACHE_OFF
#define CONFIG_CMD_CACHE
#endif

#define CONFIG_CMD_TIME
#define CONFIG_SYS_ALT_MEMTEST

/* USB Configs */
#define CONFIG_CMD_USB
#define CONFIG_CMD_FAT
#define CONFIG_USB_EHCI
#define CONFIG_USB_EHCI_MX6
#define CONFIG_USB_STORAGE
/*#define CONFIG_USB_HOST_ETHER*/
/*#define CONFIG_USB_ETHER_ASIX*/
/*#define CONFIG_USB_ETHER_MCS7830*/
/*#define CONFIG_USB_ETHER_SMSC95XX*/
#define CONFIG_USB_MAX_CONTROLLER_COUNT 2
#define CONFIG_EHCI_HCD_INIT_AFTER_RESET	/* For OTG port */
#define CONFIG_MXC_USB_PORTSC	(PORT_PTS_UTMI | PORT_PTS_PTW)
#define CONFIG_MXC_USB_FLAGS	0


/* set bootcounter */
/*#define CONFIG_BOOTCOUNT_LIMIT*/
/*#define CONFIG_SYS_BOOTCOUNT_ADDR	IRAM_BASE_ADDR*/
/*#define CONFIG_SYS_BOOTCOUNT_BE*/

/*#define CONFIG_HW_WATCHDOG*/
/*#define CONFIG_IMX_WATCHDOG*/
/*#define CONFIG_WATCHDOG_TIMEOUT_MSECS	10000*/

/*
 * POST support
#define CONFIG_POST		(CONFIG_SYS_POST_MEMORY | \
				 CONFIG_SYS_POST_I2C | \
				 CONFIG_SYS_POST_WATCHDOG)

#define CONFIG_POST_WATCHDOG  {				\
	"Watchdog timer test",				\
	"watchdog",					\
	"This test checks the watchdog timer.",		\
	POST_RAM | POST_SLOWTEST | POST_MANUAL | POST_REBOOT, \
	&watchdog_post_test,				\
	NULL,						\
	NULL,						\
	CONFIG_SYS_POST_WATCHDOG			\
	}

#define CONFIG_SYS_POST_WORD_ADDR	0x20cc068

#define CONFIG_SYS_POST_I2C_ADDRS	{ 0x08 }
 */

#endif /* __CONFIG_H */
