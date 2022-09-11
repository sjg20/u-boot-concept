/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2014 Google, Inc
 */

#ifndef _ATMEL_SERIAL_H
#define _ATMEL_SERIAL_H

struct atmel_usart3;

/* Information about a serial port */
struct atmel_serial_platdata {
	uint32_t base_addr;
};

void _atmel_serial_init(struct atmel_usart3 *usart, ulong usart_clk_rate,
			int baudrate);

#endif
