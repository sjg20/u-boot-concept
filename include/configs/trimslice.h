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

#ifndef __CONFIG_H
#define __CONFIG_H
#include <asm/sizes.h>

#undef DEBUG
/*
 * High Level Configuration Options
 */
#define CONFIG_CMD_EXT2         1
#define CONFIG_ARMCORTEXA9	1	/* This is an ARM V7 CPU core */
#define CONFIG_TEGRA2		1	/* in a NVidia Tegra2 core */
#define CONFIG_TEGRA2_TRIMSLICE		1	/* working with Harmony */
#define CONFIG_SKIP_RELOCATE_UBOOT
#define CONFIG_L2_OFF		1	/* No L2 cache */

#define CONFIG_ENABLE_CORTEXA9	1	/* enable cortex A9 core */
#ifndef CONFIG_ENABLE_CORTEXA9
  #define TEGRA2_AVP_ONLY
#endif
#undef TEGRA2_TRACE

#include <asm/arch/tegra2.h>            /* get chip and board defs */

/*
 * Display CPU and Board information
 */
#define CONFIG_DISPLAY_CPUINFO		1
#define CONFIG_DISPLAY_BOARDINFO	1

#undef CONFIG_USE_IRQ
#define CONFIG_ARCH_CPU_INIT
#define CONFIG_MISC_INIT_R
#define BOARD_LATE_INIT
#define CONFIG_SKIP_LOWLEVEL_INIT

#define CONFIG_CMDLINE_TAG		1	/* enable passing of ATAGs */

/* Environment */
/* #define CONFIG_ENV_IS_NOWHERE */
#define CONFIG_ENV_SIZE			0x10000	/* Total Size Environment */

/*
 * Size of malloc() pool
 */
#define CONFIG_SYS_MALLOC_LEN		(SZ_256K * 16)   /* 4MB  */
#define CONFIG_SYS_GBL_DATA_SIZE	128	/* bytes reserved for */
						/* initial data */

/*
 * Hardware drivers
 */

/*
 * PllX Configuration
 */
#define CONFIG_SYS_CPU_OSC_FREQUENCY    1000000        /* Set CPU clock to 1GHz */

/*
 * NS16550 Configuration
 */
#define V_NS16550_CLK			216000000	/* 216MHz (pllp_out0) */

#define CONFIG_SYS_NS16550
#define CONFIG_SYS_NS16550_SERIAL
#define CONFIG_SYS_NS16550_REG_SIZE	(-4)
#define CONFIG_SYS_NS16550_CLK		V_NS16550_CLK

/*
 * select serial console configuration
 */
#define CONFIG_CONS_INDEX	1

/* allow to overwrite serial and ethaddr */
#define CONFIG_ENV_OVERWRITE
#define CONFIG_BAUDRATE			115200
#define CONFIG_SYS_BAUDRATE_TABLE	{4800, 9600, 19200, 38400, 57600,\
					115200}

#define CONFIG_CONSOLE_MUX		1
#define CONFIG_SYS_CONSOLE_IS_IN_ENV	1
#define CONFIG_STD_DEVICES_SETTINGS	"stdin=serial\0" \
					"stdout=serial\0" \
					"stderr=serial\0"


#define CONFIG_MMC			1
#define CONFIG_TEGRA2_MMC		1
#define CONFIG_DOS_PARTITION		1
#define CONFIG_EFI_PARTITION		1

#define MMC_DEV_INSTANCES 1
#define NvEmmcx_0	NvEmmc4
#define NvEmmcx_1	0
#define NvEmmcx_2	0
#define NvEmmcx_3	0

/* commands to include */
#include <config_cmd_default.h>

#define CONFIG_CMD_NET
#define CONFIG_NET_MULTI
#define CONFIG_CMD_PING
#define CONFIG_CMD_DHCP
#define CONFIG_CMD_PCI
/*#define CONFIG_CMD_I2C		I2C serial bus support	*/

#define CONFIG_CMD_MMC		/* MMC support			*/
/* #define CONFIG_CMD_NAND		/\* NAND support			*\/ */
#define CONFIG_CMD_USB		/* USB Host support		*/

#undef CONFIG_CMD_FLASH		/* flinfo, erase, protect	*/
#undef CONFIG_CMD_FPGA		/* FPGA configuration Support	*/
#undef CONFIG_CMD_IMI		/* iminfo			*/
#undef CONFIG_CMD_IMLS		/* List all found images	*/
#undef CONFIG_CMD_NFS		/* NFS support			*/

/* turn on command-line edit/hist/auto */

#define CONFIG_CMDLINE_EDITING		1
#define CONFIG_COMMAND_HISTORY		1
#define CONFIG_AUTOCOMPLETE			1

#define CONFIG_SYS_64BIT_STRTOUL		1
#define CONFIG_SYS_64BIT_VSPRINTF		1

#define CONFIG_SYS_NO_FLASH

/*
 * Board NAND Info.
 */
/* #define CONFIG_NAND_TEGRA2 */
/* #define CONFIG_SYS_NAND_ADDR		NAND_BASE	/\* physical address *\/ */
/* 							/\* to access nand *\/ */
/* #define CONFIG_SYS_NAND_BASE		NAND_BASE	/\* physical address *\/ */
/* 							/\* to access nand at *\/ */
/* 							/\* CS0 *\/ */
/* #define CONFIG_SYS_MAX_NAND_DEVICE	1		/\* Max number of NAND *\/ */
/* 							/\* devices *\/ */

/*
 * USB Host.
 */
#define USB_EHCI_TEGRA_BASE_ADDR_USB3   0xC5008000      /* USB3 base address */

#define CONFIG_USB_EHCI
#define CONFIG_USB_EHCI_TEGRA
#define NvUSBx_0	USB_EHCI_TEGRA_BASE_ADDR_USB3
#define NvUSBx_1	0
#define NvUSBx_2	0
#define NvUSBx_3	0

/*
 */
#define CONFIG_USB_EHCI_DATA_ALIGN		4

/*
 * This parameter affects a TXFILLTUNING field that controls how much data is
 * sent to the latency fifo before it is sent to the wire. Without this
 * parameter, the default (2) causes occasional Data Buffer Errors in OUT
 * packets depending on the buffer address and size.
 */
#define CONFIG_USB_EHCI_TXFIFO_THRESH	10

#define CONFIG_EHCI_IS_TDI
#define CONFIG_USB_STORAGE
#define CONFIG_USB_HOST_ETHER
#define CONFIG_USB_ETHER_ASIX
#define CONFIG_USB_ETHER_SMSC95XX

#define TEGRA2_SYSMEM			"mem=384M@0M nvmem=128M@384M mem=512M@512M"

/* Environment information */
#define CONFIG_DEFAULT_ENV_SETTINGS \
	"console=ttyS0,115200n8\0" \
	"mem=" TEGRA2_SYSMEM "\0" \
	"smpflag=smp\0" \
	"videospec=tegrafb\0"

#define CONFIG_IPADDR		10.0.0.2
#define CONFIG_SERVERIP		10.0.0.1
#define CONFIG_LOADADDR		0x4080000 /* free RAM to download kernel to */
#define CONFIG_BOOTFILE		vmlinux.uimg
#define TEGRA_EHCI_PROBE_DELAY_DEFAULT	"5000"
#define CONFIG_BOOTDELAY		5      /* -1 to disable auto boot */

/* no auto load */
#define CONFIG_SYS_AUTOLOAD            "n"             /* No autoload */

/* try load from usb first, then mmc */
#undef CONFIG_BOOTCOMMAND	
/* #define CONFIG_BOOTCOMMAND              "run usbboot ; run mmcboot" */

#define CONFIG_AUTO_COMPLETE
/*
 * Miscellaneous configurable options
 */
#define CONFIG_SYS_LONGHELP		/* undef to save memory */
#define CONFIG_SYS_HUSH_PARSER		/* use "hush" command parser */
#define CONFIG_SYS_PROMPT_HUSH_PS2	"> "
#define CONFIG_SYS_PROMPT		V_PROMPT
/* Increasing the size of the IO buffer as default nfsargs size is more than 256
  and so it is not possible to edit it */
#define CONFIG_SYS_CBSIZE		(256 * 2) /* Console I/O Buffer Size */
/* Print Buffer Size */
#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE + \
					sizeof(CONFIG_SYS_PROMPT) + 16)
#define CONFIG_SYS_MAXARGS		16	/* max number of command args */
/* Boot Argument Buffer Size */
#define CONFIG_SYS_BARGSIZE		(CONFIG_SYS_CBSIZE)

#define CONFIG_SYS_MEMTEST_START	(TEGRA2_SDRC_CS0 + 0x600000)
								/* mem test */
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_MEMTEST_START + \
					0x02000000) /* 32MB */

#define CONFIG_SYS_LOAD_ADDR		(0xA00800)	/* default */
							/* load address */

#define CONFIG_SYS_HZ			1000

/*-----------------------------------------------------------------------
 * Stack sizes
 *
 * The stack sizes are set up in start.S using the settings below
 */
#define CONFIG_STACKBASE	0x2800000	/* 40MB */
#define CONFIG_STACKSIZE	SZ_128K	/* regular stack */
#ifdef CONFIG_USE_IRQ
#define CONFIG_STACKSIZE_IRQ	SZ_4K	/* IRQ stack */
#define CONFIG_STACKSIZE_FIQ	SZ_4K	/* FIQ stack */
#endif

/*-----------------------------------------------------------------------
 * Physical Memory Map
 */
#define CONFIG_NR_DRAM_BANKS	1
#define PHYS_SDRAM_1		TEGRA2_SDRC_CS0
#define PHYS_SDRAM_1_SIZE	SZ_512M

/*-----------------------------------------------------------------------
 * FLASH and environment organization
 */

#define CONFIG_SYS_MAX_FLASH_SECT	520	/* max number of sectors on */
						/* one chip */
#define CONFIG_SYS_MAX_FLASH_BANKS	2	/* max number of flash banks */
#define CONFIG_SYS_MONITOR_LEN		SZ_256K	/* Reserve 2 sectors */

#define CONFIG_SYS_FLASH_BASE		NAND_BASE

/* Monitor at start of flash */
#define CONFIG_SYS_MONITOR_BASE		CONFIG_SYS_FLASH_BASE

/*
 *  I2C configuration
 */
#define CONFIG_TEGRA2_I2C
#define CONFIG_CMD_I2C
#define CONFIG_I2C_MULTI_BUS	1
#define CONFIG_SYS_MAX_I2C_BUS	4
#define CONFIG_SYS_I2C_SPEED		100000
#define CONFIG_SYS_I2C_SLAVE		1
#define CONFIG_SYS_I2C_BUS		0
#define CONFIG_SYS_I2C_BUS_SELECT	1
#undef CONFIG_DRIVER_TEGRA2_I2C

/*
 * RealTek 8169
 */
#define CONFIG_PCI
#define CONFIG_PCI_PNP
#define CONFIG_PCI_CONFIG_HOST_BRIDGE
#define CONFIG_RTL8169

/*
 * High Level Configuration Options
 */
#define V_PROMPT			"TrimSlice # "

#define CONFIG_SERIAL_MULTI		1
#define CONFIG_TEGRA2_ENABLE_UARTA	1
#define CONFIG_TEGRA2_DEBUG_BAUD	115200

#define CONFIG_EXTRA_ENV_SETTINGS \
	CONFIG_STD_DEVICES_SETTINGS \
	"usbprobedelay=" TEGRA_EHCI_PROBE_DELAY_DEFAULT "\0" \
	"usbhost=on\0" \
	CONFIG_DEFAULT_ENV_SETTINGS \
	"usbboot=usb start; " \
		"ext2load usb 0:1 ${loadaddr} /uImage; " \
		"setenv bootargs root=/dev/sdb1 rw rootwait " \
                "${mem} " \
		"console=${console} " \
		"usbcore.old_scheme_first=1 " \
		"tegraboot=${tegraboot} " \
		"tegrap earlyprintk; "\
		"bootm ${loadaddr}\0" \
	"mmcboot=mmc init; " \
		"ext2load mmc 0:2 ${loadaddr} /uImage; " \
		"setenv bootargs root=/dev/sda1 rw rootwait " \
                "${mem} " \
		"console=${console} " \
		"usbcore.old_scheme_first=1 " \
		"tegraboot=${tegraboot} " \
		"tegrap earlyprintk; "\
		"bootm ${loadaddr}\0" \

/* #define CONFIG_BOOTARGS	"root=/dev/sdb1 rw rootwait mem=448M@0M mem=512M@512M video=tegrafb console=ttyS0,115200n8 tegraboot=nand earlyprintk" */

#define CONFIG_BOOTCOMMAND              "run mmcboot ; run usbboot"

/* #define CONFIG_RAM_DEBUG	1 */
/* #define TEGRA2_TRACE	1 */

/* UARTA: debug board uart */
#define CONFIG_SYS_NS16550_COM1		NV_ADDRESS_MAP_APB_UARTA_BASE

/* These config switches are for GPIO support */
#define CONFIG_TEGRA2_GPIO		1
#define CONFIG_CMD_TEGRA2_GPIO_INFO	1

#define LINUX_MACH_TYPE			MACH_TYPE_TRIMSLICE
#define CONFIG_SYS_BOARD_ODMDATA	0x300d8011

#define CONFIG_I2CP_PIN_MUX		1
#define CONFIG_I2C1_PIN_MUX		1
#define CONFIG_I2C2_PIN_MUX		1
#define CONFIG_I2C3_PIN_MUX		1

#define CONFIG_TEGRA2_SPI
#define CONFIG_SPI_FLASH
#define CONFIG_SPI_FLASH_STMICRO
#define CONFIG_SPI_FLASH_SLOW_READ
#define CONFIG_SF_DEFAULT_MODE	SPI_MODE_0
#define CONFIG_CMD_SPI
#define CONFIG_CMD_SF

#define CONFIG_ENV_SPI_BUS 0
#define CONFIG_ENV_SPI_CS 0
#define	CONFIG_ENV_SPI_MAX_HZ 6000000
#define CONFIG_ENV_SPI_MODE SPI_MODE_0

#define CONFIG_ENV_IS_IN_SPI_FLASH
#define CONFIG_ENV_SECT_SIZE	0x10000	/* 4K sectors */
#define CONFIG_ENV_OFFSET		SZ_512K

#define CONFIG_LCD
#define CONFIG_TEGRA2_LCD
#define LCD_BPP             LCD_COLOR16
#define LCD_FB_ADDR         0x1C022000   /* FB could be passed from bl */
#define CONFIG_SYS_WHITE_ON_BLACK       /*Console colors*/

#define LCD_vl_col	1024
#define LCD_vl_row	768

#define CONFIG_KEYBOARD		1
#define CONFIG_USB_KEYBOARD	1

#endif /* __CONFIG_H */
