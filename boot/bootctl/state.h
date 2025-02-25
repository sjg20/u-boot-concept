/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Uclass implementation for boot schema
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#ifndef __bootctl_state_h
#define __bootctl_state_h

#include <alist.h>

struct udevice;

/**
 * struct state - State information which can be read and written
 *
 * @bflow: Bootflow for this OS
 */
struct bc_state {
	struct alist subnodes;
};

/**
 * struct bc_state_ops - Operations for displays
 */
struct bc_state_ops {
	/**
	 * first() - Find the first available OS
	 *
	 * @info: Returns info on success
	 * Return 0 if OK, -ENOENT if there is no OS to boot
	 */
	int (*first)(struct udevice *dev);

};

#define bc_state_get_ops(dev)  ((struct bc_state_ops *)(dev)->driver->ops)

#endif
