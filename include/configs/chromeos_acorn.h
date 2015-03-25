/*
 * Copyright (c) 2015, Google Inc. All rights reserved.
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

#define CONFIG_CUSTOMER_BOARD_0
#define CONFIG_ARMADA_38X

#define CONFIG_BOOTARGS
#define CONFIG_BOOTSTAGE
#define CONFIG_OF_CONTROL

#define SPL_RUNNING_FROM_UBOOT 0

#define MV_SPI_BOOT
#define MV_INCLUDE_SPI
#define MV_INCLUDE_NOR

#define CONFIG_PHYSMEM

#undef CONFIG_CHROMEOS_DISPLAY
#define CONFIG_CHROMEOS_SPI

#define CONFIG_SPI
#define CONFIG_SPI_FLASH
#define CONFIG_CMD_SPI
#define CONFIG_CMD_SF

#undef CONFIG_CMD_EEPROM

#include <configs/armada_38x.h>
#include <configs/chromeos.h>

#undef CONFIG_CHROMEOS_TEST
#undef CONFIG_CROS_EC
