/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

/* Implementation of per-board GPIO accessor functions */

#include <common.h>
#include <fdt_decode.h>
#include <asm/arch/gpio.h>
#include <asm/arch/tegra2.h>
#include <asm/global_data.h>
#include <chromeos/common.h>
#include <chromeos/cros_gpio.h>
#include <asm/arch/pinmux.h>

#define PREFIX "cros_gpio: "

DECLARE_GLOBAL_DATA_PTR;

int cros_gpio_fetch(enum cros_gpio_index index, cros_gpio_t *gpio)
{
	const char const *port[CROS_GPIO_MAX_GPIO] = {
		"gpio_port_write_protect_switch",
		"gpio_port_recovery_switch",
		"gpio_port_developer_switch",
		"gpio_port_lid_switch",
		"gpio_port_power_switch",
		"gpio_port_ec_reset",
	};
	const char const *polarity[CROS_GPIO_MAX_GPIO] = {
		"polarity_write_protect_switch",
		"polarity_recovery_switch",
		"polarity_developer_switch",
		"polarity_lid_switch",
		"polarity_power_switch",
		"polarity_ec_reset",
	};
	int p;

	if (index < 0 || index >= CROS_GPIO_MAX_GPIO) {
		VBDEBUG(PREFIX "index out of range: %d\n", index);
		return -1;
	}

	gpio->index = index;

	gpio->port = fdt_decode_get_config_int(gd->blob, port[index], -1);
	if (gpio->port == -1) {
		VBDEBUG(PREFIX "failed to decode gpio port\n");
		return -1;
	}

	gpio_direction_input(gpio->port);

	gpio->polarity =
		fdt_decode_get_config_int(gd->blob, polarity[index], -1);
	if (gpio->polarity == -1) {
		VBDEBUG(PREFIX "failed to decode gpio polarity\n");
		return -1;
	}

	p = (gpio->polarity == CROS_GPIO_ACTIVE_HIGH) ? 0 : 1;
	gpio->value = p ^ gpio_get_value(gpio->port);

	return 0;
}

int cros_gpio_set(enum cros_gpio_index index, int value)
{
	int gpio_pin;
	int gpio_polarity;
	const char const *port[CROS_GPIO_MAX_GPIO] = {
		"gpio_port_write_protect_switch",
		"gpio_port_recovery_switch",
		"gpio_port_developer_switch",
		"gpio_port_lid_switch",
		"gpio_port_power_switch",
		"gpio_port_ec_reset",
	};
	const char const *polarity[CROS_GPIO_MAX_GPIO] = {
		"polarity_write_protect_switch",
		"polarity_recovery_switch",
		"polarity_developer_switch",
		"polarity_lid_switch",
		"polarity_power_switch",
		"polarity_ec_reset",
	};

	if (index < 0 || index >= CROS_GPIO_MAX_GPIO) {
		VBDEBUG(PREFIX "index out of range: %d\n", index);
		return -1;
	}

	gpio_pin = fdt_decode_get_config_int(gd->blob, port[index], -1);
	if (gpio_pin == -1) {
		VBDEBUG(PREFIX "failed to decode gpio port\n");
		return -1;
	}

	gpio_polarity =
		fdt_decode_get_config_int(gd->blob, polarity[index], -1);
	if (gpio_polarity == -1) {
		VBDEBUG(PREFIX "failed to decode gpio polarity\n");
		return -1;
	}

	/* Currently this assumes the GPIO is set in the pinmux.
	 * TODO: Expand to support configuration for to enable pinmux
	 * settings here.
	 */
	gpio_direction_output(gpio_pin, (gpio_polarity == value));
	gpio_set_value(gpio_pin, (gpio_polarity == value));
	VBDEBUG(PREFIX "Set GPIO %d to %d \n", gpio_pin, value);

	return 0;
}

int cros_gpio_dump(cros_gpio_t *gpio)
{
#ifdef VBOOT_DEBUG
	const char const *name[CROS_GPIO_MAX_GPIO] = {
		"wpsw", "recsw", "devsw", "lidsw", "pwrsw"
	};
	int index = gpio->index;

	if (index < 0 || index >= CROS_GPIO_MAX_GPIO) {
		VBDEBUG(PREFIX "index out of range: %d\n", index);
		return -1;
	}

	VBDEBUG(PREFIX "%-6s: port=%3d, polarity=%d, value=%d\n",
			name[gpio->index],
			gpio->port, gpio->polarity, gpio->value);
#endif
	return 0;
}

int cros_check_for_ec_reset_gpio(){
	int ec_reset;
	ec_reset = fdt_decode_get_config_int(gd->blob,
		"hold_ec_in_reset_during_uboot", -1);
	if (ec_reset != 1) {
		VBDEBUG(PREFIX "no EC reset flag present\n");
		return -1;
	}
	/* Setup pin muxing for the EC reset pin, note that this assumes the
	 * EC reset GPIO is on the DAP2 pingroup on Tegra2.
	 * TODO find a better place or way of managing the pinmux.
	 */
	pinmux_set_func(PINGRP_DAP2, PMUX_FUNC_DAP2);
	pinmux_set_pullupdown(PINGRP_DAP2, PMUX_PULL_NORMAL);
	pinmux_set_tristate(PINGRP_DAP2, PMUX_TRI_NORMAL);
	return 0;
}
