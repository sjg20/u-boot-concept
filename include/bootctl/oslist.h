/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Uclass implementation for boot schema
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#ifndef __bootctl_oslist_h
#define __bootctl_oslist_h

#include <bootflow.h>

struct udevice;

/**
 * struct osinfo - Information about an OS which is available for booting
 *
 * @bflow: Bootflow for this OS
 */
struct osinfo {
	struct bootflow bflow;
};

/**
 * struct oslist_iter - Iterator
 *
 * @active: true if the scan has started
 * @bf_iter: bootstd iterator being used
 */
struct oslist_iter {
	bool active;
	struct bootflow_iter bf_iter;
};

/**
 * struct bc_oslist_ops - Operations for displays
 */
struct bc_oslist_ops {
	/**
	 * next() - Find the next available OS
	 *
	 * @info: Returns info on success
	 * Return 0 if OK, -ENOENT if there are no more
	 */
	int (*next)(struct udevice *dev, struct oslist_iter *iter,
		    struct osinfo *info);
};

#define bc_oslist_get_ops(dev)  ((struct bc_oslist_ops *)(dev)->driver->ops)

/**
 * bc_oslist_setup_iter() - Set up a new iterator ready for use
 *
 * This must be called before using the iterator with bc_oslist_next()
 *
 * @iter: Pointer to iterator to set up
 */
void bc_oslist_setup_iter(struct oslist_iter *iter);

/**
 * bc_oslist_next() - Find the next available OS
 *
 * @info: Returns info on success
 * Return 0 if OK, -ENOENT if there are no more
 */
int bc_oslist_next(struct udevice *dev, struct oslist_iter *iter,
		   struct osinfo *info);

#endif
