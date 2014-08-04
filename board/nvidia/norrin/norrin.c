/*
 * (C) Copyright 2014
 * NVIDIA Corporation <www.nvidia.com>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <asm/arch/gpio.h>
#include <asm/arch/pinmux.h>
#include "pinmux-config-norrin.h"

/*
 * Routine: pinmux_init
 * Description: Do individual peripheral pinmux configs
 */
void pinmux_init(void)
{
	pinmux_set_tristate_input_clamping();

	gpio_config_table(norrin_gpio_inits,
			  ARRAY_SIZE(norrin_gpio_inits));

	pinmux_config_pingrp_table(norrin_pingrps,
				   ARRAY_SIZE(norrin_pingrps));

	pinmux_config_drvgrp_table(norrin_drvgrps,
				   ARRAY_SIZE(norrin_drvgrps));
}
