/*
 * Copyright (c) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _CLK_H_
#define _CLK_H_

#include <linux/types.h>

struct udevice;

int soc_clk_dump(void);

struct clk_ops {
	/**
	 * get_rate() - Get current clock rate
	 *
	 * @dev:	Device to check (UCLASS_CLK)
	 * @return clock rate in Hz, or -ve error code
	 */
	ulong (*get_rate)(struct udevice *dev);

	/**
	 * set_rate() - Set current clock rate
	 *
	 * @dev:	Device to adjust
	 * @rate:	New clock rate in Hz
	 * @return new rate, or -ve error code
	 */
	ulong (*set_rate)(struct udevice *dev, ulong rate);

	/**
	 * get_periph_rate() - Get clock rate for a peripheral
	 *
	 * @dev:	Device to check (UCLASS_CLK)
	 * @periph:	Peripheral ID to check
	 * @return clock rate in Hz, or -ve error code
	 */
	ulong (*get_periph_rate)(struct udevice *dev, int periph);

	/**
	 * set_periph_rate() - Set current clock rate for a peripheral
	 *
	 * @dev:	Device to update (UCLASS_CLK)
	 * @periph:	Peripheral ID to update
	 * @return new clock rate in Hz, or -ve error code
	 */
	ulong (*set_periph_rate)(struct udevice *dev, int periph, ulong rate);

	/**
	 * get_id() - Get peripheral ID
	 *
	 * @dev:	clock provider
	 * @args_count:	number of arguments
	 * @args:	arguments.  The meaning is driver specific.
	 * @return peripheral ID, or -ve error code
	 */
	int (*get_id)(struct udevice *dev, int args_count, uint32_t *args);
};

#define clk_get_ops(dev)	((struct clk_ops *)(dev)->driver->ops)

/**
 * clk_get_rate() - Get current clock rate
 *
 * @dev:	Device to check (UCLASS_CLK)
 * @return clock rate in Hz, or -ve error code
 */
ulong clk_get_rate(struct udevice *dev);

/**
 * clk_set_rate() - Set current clock rate
 *
 * @dev:	Device to adjust
 * @rate:	New clock rate in Hz
 * @return new rate, or -ve error code
 */
ulong clk_set_rate(struct udevice *dev, ulong rate);

/**
 * clk_get_periph_rate() - Get current clock rate for a peripheral
 *
 * @dev:	Device to check (UCLASS_CLK)
 * @return clock rate in Hz, -ve error code
 */
ulong clk_get_periph_rate(struct udevice *dev, int periph);

/**
 * clk_set_periph_rate() - Set current clock rate for a peripheral
 *
 * @dev:	Device to update (UCLASS_CLK)
 * @periph:	Peripheral ID to update
 * @return new clock rate in Hz, or -ve error code
 */
ulong clk_set_periph_rate(struct udevice *dev, int periph, ulong rate);

/**
 * clk_get_id() - Get peripheral ID
 *
 * @dev:	clock provider
 * @args_count:	number of arguments
 * @args:	arguments.  The meaning is driver specific.
 * @return peripheral ID, or -ve error code
 */
int clk_get_id(struct udevice *dev, int args_count, uint32_t *args);

/**
 * clk_get_id_simple() - Simple implementation of get_id() callback
 *
 * @dev:	clock provider
 * @args_count:	number of arguments
 * @args:	arguments.
 * @return peripheral ID, or -ve error code
 */
static inline int clk_get_id_simple(struct udevice *dev, int args_count,
				    uint32_t *args)
{
	return args_count > 0 ? args[0] : 0;
}

#endif /* _CLK_H_ */
