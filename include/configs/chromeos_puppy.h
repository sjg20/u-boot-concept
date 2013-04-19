/*
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CHROMEOS_PUPPY_CONFIG_H
#define __CHROMEOS_PUPPY_CONFIG_H

/* Add chromeos specific only for non spl build */
#ifndef CONFIG_SPL_BUILD

#define CONFIG_DIRECT_BOOTARGS ""
#define CONFIG_STD_DEVICES_SETTINGS ""
#include <configs/chromeos.h>

#define CONFIG_PHYSMEM
#define CONFIG_CROS_EC
#define CONFIG_TPM
#define CONFIG_INFINEON_TPM_I2C

#endif

#include <configs/dalmore.h>

/* High-level configuration options */
#ifdef V_PROMPT
#undef V_PROMPT
#endif
#define V_PROMPT		"Tegra114 (Puppy) # "

#ifdef CONFIG_TEGRA_BOARD_STRING
#undef CONFIG_TEGRA_BOARD_STRING
#endif
#define CONFIG_TEGRA_BOARD_STRING	"NVIDIA Puppy"

#endif /* __CHROMEOS_PUPPY_CONFIG_H */
