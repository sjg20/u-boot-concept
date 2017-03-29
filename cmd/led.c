/*
 * (C) Copyright 2010
 * Jason Kridner <jkridner@beagleboard.org>
 *
 * Based on cmd_led.c patch from:
 * http://www.mail-archive.com/u-boot@lists.denx.de/msg06873.html
 * (C) Copyright 2008
 * Ulf Samuelsson <ulf.samuelsson@atmel.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <config.h>
#include <command.h>
#include <status_led.h>

struct led_tbl_s {
	char		*string;	/* String for use in the command */
	enum led_id_t	id;
};

typedef struct led_tbl_s led_tbl_t;

static const led_tbl_t led_commands[] = {
#ifdef CONFIG_LED_STATUS_BOARD_SPECIFIC
#ifdef CONFIG_LED_STATUS0
	{ "0", CONFIG_LED_STATUS_BIT },
#endif
#ifdef CONFIG_LED_STATUS1
	{ "1", CONFIG_LED_STATUS_BIT1 },
#endif
#ifdef CONFIG_LED_STATUS2
	{ "2", CONFIG_LED_STATUS_BIT2 },
#endif
#ifdef CONFIG_LED_STATUS3
	{ "3", CONFIG_LED_STATUS_BIT3 },
#endif
#ifdef CONFIG_LED_STATUS4
	{ "4", CONFIG_LED_STATUS_BIT4 },
#endif
#ifdef CONFIG_LED_STATUS5
	{ "5", CONFIG_LED_STATUS_BIT5 },
#endif
#endif
#ifdef CONFIG_LED_STATUS_GREEN
	{ "green", LED_GREEN },
#endif
#ifdef CONFIG_LED_STATUS_YELLOW
	{ "yellow", LED_YELLOW },
#endif
#ifdef CONFIG_LED_STATUS_RED
	{ "red", LED_RED },
#endif
#ifdef CONFIG_LED_STATUS_BLUE
	{ "blue", LED_BLUE },
#endif
	{ NULL, 0 }
};

enum led_action_t get_led_cmd(char *var)
{
	if (strcmp(var, "off") == 0)
		return LED_OFF;
	if (strcmp(var, "on") == 0)
		return LED_ON;
	if (strcmp(var, "toggle") == 0)
		return LED_TOGGLE;
	if (strcmp(var, "blink") == 0)
		return LED_BLINK;

	return LED_NONE;
}

/*
 * LED drivers providing a blinking LED functionality, like the
 * PCA9551, can override this empty weak function
 */
void __weak led_set_blink(enum led_colour_t colour, int freq)
{
}

int do_led (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int i, match = 0;
	enum led_action_t cmd;
	int freq = 0;

	/* Validate arguments */
	if ((argc < 3) || (argc > 4))
		return CMD_RET_USAGE;

	cmd = get_led_cmd(argv[2]);
	if (cmd < 0) {
		return CMD_RET_USAGE;
	} else if (cmd == LED_BLINK) {
		if (argc != 4)
			return CMD_RET_USAGE;
		freq = simple_strtoul(argv[3], NULL, 10);
	}

	for (i = 0; led_commands[i].string; i++) {
		const struct led_tbl_s *led = &led_commands[i];

		if ((strcmp("all", argv[1]) == 0) ||
		    (strcmp(led->string, argv[1]) == 0)) {
			match = 1;
			if (cmd == LED_BLINK)
				led_set_blink(led->mask, freq);
			else
				led_set_state(led->mask, cmd);

			/* Need to set only 1 led if led_name wasn't 'all' */
			if (strcmp("all", argv[1]) != 0)
				break;
		}
	}

	/* If we ran out of matches, print Usage */
	if (!match) {
		return CMD_RET_USAGE;
	}

	return 0;
}

U_BOOT_CMD(
	led, 4, 1, do_led,
	"["
#ifdef CONFIG_LED_STATUS_BOARD_SPECIFIC
#ifdef CONFIG_LED_STATUS0
	"0|"
#endif
#ifdef CONFIG_LED_STATUS1
	"1|"
#endif
#ifdef CONFIG_LED_STATUS2
	"2|"
#endif
#ifdef CONFIG_LED_STATUS3
	"3|"
#endif
#ifdef CONFIG_LED_STATUS4
	"4|"
#endif
#ifdef CONFIG_LED_STATUS5
	"5|"
#endif
#endif
#ifdef CONFIG_LED_STATUS_GREEN
	"green|"
#endif
#ifdef CONFIG_LED_STATUS_YELLOW
	"yellow|"
#endif
#ifdef CONFIG_LED_STATUS_RED
	"red|"
#endif
#ifdef CONFIG_LED_STATUS_BLUE
	"blue|"
#endif
	"all] [on|off|toggle|blink] [blink-freq in ms]",
	"[led_name] [on|off|toggle|blink] sets or clears led(s)"
);
