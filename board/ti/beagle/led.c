/*
 * Copyright (c) 2010 Texas Instruments, Inc.
 * Jason Kridner <jkridner@beagleboard.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <status_led.h>
#include <asm/arch/cpu.h>
#include <asm/io.h>
#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>

/* GPIO pins for the LEDs */
#define BEAGLE_LED_USR0	150
#define BEAGLE_LED_USR1	149

static int gpio_for_colour[LED_COLOUR_COUNT] = {
	[LED_RED] = BEAGLE_LED_USR0,
	[LED_GREEN] = BEAGLE_LED_USR1,
};

int led_set_state(enum led_colour_t colour, enum led_action_t action)
{
	int gpio = gpio_for_colour[colour];
	int state;

	if (!gpio)
		return -EINVAL;
	switch (action) {
	case LED_OFF:
	case LED_ON:
		gpio_direction_output(gpio, action);
		break;
	case LED_TOGGLE:
		state = gpio_get_value(gpio);
		gpio_direction_output(gpio, !state);
	default:
		return -ENOSYS;
	}

	return 0;
}
