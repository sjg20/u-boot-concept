/*
 *  (C) Copyright 2010-2012
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

/* Enable fdt support for TrimSlice. Flash the image in u-boot-dtb.bin */
#define CONFIG_DEFAULT_DEVICE_TREE	tegra2-trimslice
#define CONFIG_OF_CONTROL
#define CONFIG_OF_SEPARATE

/* High-level configuration options */
#define V_PROMPT		"Tegra2 (TrimSlice) # "
#define CONFIG_TEGRA2_BOARD_STRING	"Compulab Trimslice"

/* Board-specific serial config */
#define CONFIG_SERIAL_MULTI
#define CONFIG_TEGRA2_ENABLE_UARTA
#define CONFIG_TEGRA2_UARTA_GPU
#define CONFIG_SYS_NS16550_COM1		NV_PA_APB_UARTA_BASE

#define CONFIG_MACH_TYPE		MACH_TYPE_TRIMSLICE

#define CONFIG_BOARD_EARLY_INIT_F

/* SPI */
#define CONFIG_TEGRA_SPI
#define CONFIG_SPI_FLASH
#define CONFIG_SPI_FLASH_WINBOND
#define CONFIG_SF_DEFAULT_MODE		SPI_MODE_0
#define CONFIG_CMD_SPI
#define CONFIG_CMD_SF

/* I2C */
#define CONFIG_TEGRA_I2C
#define CONFIG_SYS_I2C_INIT_BOARD
#define CONFIG_I2C_MULTI_BUS
#define CONFIG_SYS_MAX_I2C_BUS		4
#define CONFIG_SYS_I2C_SPEED		100000
#define CONFIG_CMD_I2C

/* SD/MMC */
#define CONFIG_MMC
#define CONFIG_GENERIC_MMC
#define CONFIG_TEGRA_MMC
#define CONFIG_CMD_MMC

#define CONFIG_DOS_PARTITION
#define CONFIG_EFI_PARTITION
#define CONFIG_CMD_EXT2
#define CONFIG_CMD_FAT

#define TEGRA2_MMC_DEFAULT_DEVICE	"0"
#define CONFIG_NET_MULTI
#define CONFIG_CMD_PING
#define CONFIG_CMD_DHCP
#define CONFIG_CMD_PCI
/*
 * RealTek 8169
 */
#define CONFIG_PCI
#define CONFIG_PCI_PNP
#define CONFIG_PCI_CONFIG_HOST_BRIDGE
#define CONFIG_RTL8169

/* Environment in SPI */
#define CONFIG_ENV_IS_IN_SPI_FLASH
#define CONFIG_ENV_SPI_MAX_HZ		48000000
#define CONFIG_ENV_SPI_MODE		SPI_MODE_0
#define CONFIG_ENV_SECT_SIZE		CONFIG_ENV_SIZE
#define CONFIG_ENV_OFFSET		(512 * 1024)

/* USB Host support */
#define CONFIG_USB_EHCI
#define CONFIG_USB_EHCI_TEGRA
#define CONFIG_USB_STORAGE
#define CONFIG_CMD_USB

#define CONFIG_TEGRA2_USB0	NV_PA_USB3_BASE
#define CONFIG_TEGRA2_USB1	NV_PA_USB1_BASE
#define CONFIG_TEGRA2_USB2	0
#define CONFIG_TEGRA2_USB3	0
#define CONFIG_TEGRA2_USB1_HOST

/* USB networking support */
#define CONFIG_USB_HOST_ETHER
#define CONFIG_USB_ETHER_ASIX

/* General networking support */
#define CONFIG_CMD_NET
#define CONFIG_CMD_DHCP

/* rtl8169 recv timeout issue solution */
#define NUM_RX_DESC 1

#include "tegra2-common-post.h"

#define TEGRA2_SYSMEM			"mem=384M@0M nvmem=128M@384M mem=512M@512M"

/* Environment information */
#define CONFIG_DEFAULT_ENV_SETTINGS \
	"console=ttyS0,115200n8\0" \
	"mem=" TEGRA2_SYSMEM "\0" \
	"smpflag=smp\0" \
	"videospec=tegrafb\0"

#define CONFIG_STD_DEVICES_SETTINGS	"stdin=serial,usbkbd\0" \
					"stdout=serial,lcd\0" \
					"stderr=serial,lcd\0"

#define CONFIG_NET_ENV_SETTINGS "autoload=n\0"

#undef CONFIG_EXTRA_ENV_SETTINGS

#define CONFIG_EXTRA_ENV_SETTINGS   \
	CONFIG_STD_DEVICES_SETTINGS \
	CONFIG_DEFAULT_ENV_SETTINGS \
	CONFIG_NET_ENV_SETTINGS \
	"boot_file=boot.scr\0" \
	"boot_file_load_cmd=source ${loadaddr};\0" \
	"start_bus=${interface} ${interface_init_cmd} ${bus}; \0" \
	"scan_device=for i in / /boot/; do " \
		      "for j in fat ext2; do " \
			  "setenv prefix $i;" \
			  "setenv fs $j;" \
			  "echo Scanning ${fs} ${interface} ${device} on prefix ${prefix} ...;" \
			  "if ${fs}load ${interface} ${device} ${loadaddr} ${prefix}${boot_file}; then " \
			      "echo ${boot_file} found! Executing ...;" \
			      "run boot_file_load_cmd;" \
			    "fi;" \
		      "done;" \
		     "done;\0" \
	"scan_boot=setenv interface mmc; setenv interface_init_cmd dev; setenv device 0; " \
		    "echo Scanning MMC card ...; setenv bus 0; run start_bus; run scan_device; " \
		    "setenv interface usb; setenv interface_init_cmd start; setenv device 0; " \
		    "echo Scanning USB key ...; setenv bus 0; run start_bus; run scan_device; " \
		    "setenv interface mmc; setenv interface_init_cmd dev; setenv device 1; " \
		    "echo Scanning microSD card ...; setenv bus 1; run start_bus; run scan_device; " \
		    "setenv interface usb; setenv interface_init_cmd start; setenv device 0; " \
		    "echo Scanning SSD ...; setenv bus 1; run start_bus; run scan_device;\0"

#undef CONFIG_BOOTCOMMAND
#undef CONFIG_BOOTARGS

#define CONFIG_BOOTARGS "mem=384M@0M mem=512M@512M nvmem=128M@384M vmalloc=248M video=tegrafb console=ttyS0,115200n8 rw root=/dev/sda1 nohdparm rootwait"
#define CONFIG_BOOTCOMMAND "run scan_boot"

#endif /* __CONFIG_H */
