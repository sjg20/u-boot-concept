/*
 * (C) Copyright 2000-2004
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/*
 * The purpose of this code is to signal the operational status of a
 * target which usually boots over the network; while running in
 * PCBoot, a status LED is blinking. As soon as a valid BOOTP reply
 * message has been received, the LED is turned off. The Linux
 * kernel, once it is running, will start blinking the LED again,
 * with another frequency.
 */

#ifndef _STATUS_LED_H_
#define	_STATUS_LED_H_

#ifdef CONFIG_LED_STATUS

#define LED_STATUS_PERIOD	(CONFIG_SYS_HZ / CONFIG_LED_STATUS_FREQ)
#ifdef CONFIG_LED_STATUS1
#define LED_STATUS_PERIOD1	(CONFIG_SYS_HZ / CONFIG_LED_STATUS_FREQ1)
#endif /* CONFIG_LED_STATUS1 */
#ifdef CONFIG_LED_STATUS2
#define LED_STATUS_PERIOD2	(CONFIG_SYS_HZ / CONFIG_LED_STATUS_FREQ2)
#endif /* CONFIG_LED_STATUS2 */
#ifdef CONFIG_LED_STATUS3
#define LED_STATUS_PERIOD3	(CONFIG_SYS_HZ / CONFIG_LED_STATUS_FREQ3)
#endif /* CONFIG_LED_STATUS3 */
#ifdef CONFIG_LED_STATUS4
#define LED_STATUS_PERIOD4	(CONFIG_SYS_HZ / CONFIG_LED_STATUS_FREQ4)
#endif /* CONFIG_LED_STATUS4 */
#ifdef CONFIG_LED_STATUS5
#define LED_STATUS_PERIOD5	(CONFIG_SYS_HZ / CONFIG_LED_STATUS_FREQ5)
#endif /* CONFIG_LED_STATUS5 */

void status_led_init(void);
void status_led_tick (unsigned long timestamp);
void status_led_set  (int led, int state);

#endif	/* CONFIG_LED_STATUS	*/

/*
 * Coloured LEDs API
 */
#ifndef	__ASSEMBLY__
enum led_id_t {
	LED_RED,
	LED_GREEN,
	LED_YELLOW,
	LED_BLUE,

	LED_COLOUR_COUNT,
};

enum led_action_t {
	LED_OFF = 0,
	LED_ON = 1,
	LED_TOGGLE,
	LED_BLINK,

	LED_NONE = -1,
};

void coloured_led_init(void);
int led_set_state(enum led_colour_t colour, enum led_action_t action);
#else
	.extern LED_init
	.extern red_led_on
	.extern red_led_off
	.extern yellow_led_on
	.extern yellow_led_off
	.extern green_led_on
	.extern green_led_off
	.extern blue_led_on
	.extern blue_led_off
#endif

#endif	/* _STATUS_LED_H_	*/
