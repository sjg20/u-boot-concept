// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Copyright (c) 2023, Svyatoslav Ryhel <clamor95@gmail.com>
 */

#include <common.h>
#include <env.h>
#include <fdt_support.h>
#include <asm/arch/pinmux.h>
#include <asm/arch/gp_padctrl.h>
#include <asm/arch-tegra/fuse.h>

#include "pinmux-config-tegratab.h"

/*
 * Routine: pinmux_init
 * Description: Do individual peripheral pinmux configs
 */
void pinmux_init(void)
{
	pinmux_config_pingrp_table(tegra114_tegratab_pinmux,
		ARRAY_SIZE(tegra114_tegratab_pinmux));

	/* Initialize any non-default pad configs (APB_MISC_GP regs) */
	pinmux_config_drvgrp_table(tegratab_padctrl,
		ARRAY_SIZE(tegratab_padctrl));
}

#if defined(CONFIG_OF_LIBFDT) && defined(CONFIG_OF_BOARD_SETUP)
int ft_board_setup(void *blob, struct bd_info *bd)
{
	/* Remove TrustZone nodes */
	fdt_del_node_and_alias(blob, "/firmware");
	fdt_del_node_and_alias(blob, "/reserved-memory/trustzone@bfe00000");

	/* Remove TN7 bootloader restrictions */
	fdt_del_node_and_alias(blob, "/chosen");

	return 0;
}
#endif

void nvidia_board_late_init(void)
{
	char serialno_str[17];

	/* Set chip id as serialno */
	sprintf(serialno_str, "%016llx", tegra_chip_uid());
	env_set("serial#", serialno_str);
	env_set("platform", "Tegra 4 T114");
}
