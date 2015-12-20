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
	 * enable() - Enable the clock for a peripheral
	 *
	 * @dev:	clock provider
	 * @periph:	Peripheral ID to enable
	 * @return zero on success, or -ve error code
	 */
	int (*enable)(struct udevice *dev, int periph);

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

#if CONFIG_IS_ENABLED(OF_CONTROL)
/**
 * fdt_clk_get() - Get peripheral ID from device tree
 *
 * @fdt:	FDT blob
 * @periph:	Offset of clock consumer node
 * @index:	index of a phandle to parse out in "clocks" property
 * @dev:	if not NULL, filled with pointer of clock provider
 * @return peripheral ID, or -ve error code
 */
int fdt_clk_get(const void *fdt, int nodeoffset, int index,
		struct udevice **dev);
#else
static inline int fdt_clk_get(const void *fdt, int nodeoffset, int index,
			      struct udevice **dev);
{
	return -ENOSYS;
}
#endif

/**
 * clk_get_by_index() - look up a clock referenced by a device
 *
 * Parse a device's 'clocks' list, returning information on the indexed clock,
 * ensuring that it is activated.
 *
 * @dev:	Device containing the clock reference
 * @index:	Clock index to return (0 = first)
 * @clk_devp:	Returns clock device
 * @periphp:	Returns peripheral ID for the device to control. This is the
 *		first argument after the clock node phandle.
 * @return 0 if OK, or -ve error code
 */
int clk_get_by_index(struct udevice *dev, int index, struct udevice **clk_devp,
		     int *periphp);
#endif /* _CLK_H_ */
