/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __bootdevice_h
#define __bootdevice_h

#include <linux/list.h>

struct bootflow;
struct bootflow_iter;
struct bootdevice_state;
struct udevice;

/**
 * struct bootdevice_state - information about available bootflows, etc.
 *
 * This is attached to the bootdevice uclass so there is only one of them. It
 * provides overall information about bootdevices and bootflows.
 *
 * @cur_bootdevice: Currently selected bootdevice (for commands)
 * @cur_bootflow: Currently selected bootflow (for commands)
 * @glob_head: Head for the global list of all bootdevices across all bootflows
 */
struct bootdevice_state {
	struct udevice *cur_bootdevice;
	struct bootflow *cur_bootflow;
	struct list_head glob_head;
};

/**
 * struct bootdevice_uc_plat - uclass information about a bootdevice
 *
 * This is attached to each device in the bootdevice uclass and accessible via
 * dev_get_uclass_plat(dev)
 *
 * @bootflows: List of available bootflows for this bootdevice
 */
struct bootdevice_uc_plat {
	struct list_head bootflow_head;
};

/**
 * struct bootdevice_ops - Operations for the bootdevice uclass
 *
 * Consider using ioctl() to add rarely used or driver-specific operations.
 */
struct bootdevice_ops {
	/**
	 * get_bootflow() - get a bootflow
	 *
	 * @dev:	Bootflow device to check
	 * @iter:	Provides current dev, part, method to get. Should update
	 *	max_part if there is a partition table
	 * @bflow:	Updated bootflow if found
	 * @return 0 if OK, -ESHUTDOWN if there are no more bootflows on this
	 *	device, -ENOSYS if this device doesn't support bootflows,
	 *	other -ve value on other error
	 */
	int (*get_bootflow)(struct udevice *dev, struct bootflow_iter *iter,
			    struct bootflow *bflow);
};

#define bootdevice_get_ops(dev)  ((struct bootdevice_ops *)(dev)->driver->ops)

/**
 * bootdevice_get_bootflow() - get a bootflow
 *
 * @dev:	Bootflow device to check
 * @iter:	Provides current  part, method to get
 * @bflow:	Returns bootflow if found
 * @return 0 if OK, -ESHUTDOWN if there are no more bootflows on this device,
 *	-ENOSYS if this device doesn't support bootflows, other -ve value on
 *	other error
 */
int bootdevice_get_bootflow(struct udevice *dev, struct bootflow_iter *iter,
			    struct bootflow *bflow);

/**
 * bootdevice_bind() - Bind a new named bootdevice device
 *
 * @parent:	Parent of the new device
 * @drv_name:	Driver name to use for the bootdevice device
 * @name:	Name for the device (parent name is prepended)
 * @devp:	the new device (which has not been probed)
 */
int bootdevice_bind(struct udevice *parent, const char *drv_name,
		    const char *name, struct udevice **devp);

/**
 * bootdevice_find_in_blk() - Find a bootdevice in a block device
 *
 * @dev: Bootflow device associated with this block device
 * @blk: Block device to search
 * @iter:	Provides current dev, part, method to get. Should update
 *	max_part if there is a partition table
 * @bflow: On entry, provides information about the partition and device to
 *	check. On exit, returns bootflow if found
 * @return 0 if found, -ESHUTDOWN if no more bootflows, other -ve on error
 */
int bootdevice_find_in_blk(struct udevice *dev, struct udevice *blk,
			   struct bootflow_iter *iter, struct bootflow *bflow);

/**
 * bootdevice_list() - List all available bootdevices
 *
 * @probe: true to probe devices, false to leave them as is
 */
void bootdevice_list(bool probe);

/**
 * bootdevice_get_state() - Get the (single) state for the bootdevice system
 *
 * The state holds a global list of all bootflows that have been found.
 *
 * @return 0 if OK, -ve if the uclass does not exist
 */
int bootdevice_get_state(struct bootdevice_state **statep);

/**
 * bootdevice_clear_bootflows() - Clear bootflows from a bootdevice
 *
 * Each bootdevice maintains a list of discovered bootflows. This provides a
 * way to clear it. These bootflows are removed from the global list too.
 *
 * @dev: bootdevice device to update
 */
void bootdevice_clear_bootflows(struct udevice *dev);

/**
 * bootdevice_clear_glob() - Clear the global list of bootflows
 *
 * This removes all bootflows globally and across all bootdevices.
 */
void bootdevice_clear_glob(void);

/**
 * bootdevice_add_bootflow() - Add a bootflow to the bootdevice's list
 *
 * All fields in @bflow must be set up. Note that @bflow->dev is used to add the
 * bootflow to that device.
 *
 * @dev: Bootdevice device to add to
 * @bflow: Bootflow to add. Note that fields within bflow must be allocated
 *	since this function takes over ownership of these. This functions makes
 *	a copy of @bflow itself (without allocating its fields again), so the
 *	caller must dispose of the memory used by the @bflow pointer itself
 * @return 0 if OK, -ENOMEM if out of memory
 */
int bootdevice_add_bootflow(struct bootflow *bflow);

/**
 * bootdevice_first_bootflow() - Get the first bootflow from a bootdevice
 *
 * Returns the first bootflow attached to a bootdevice
 *
 * @dev: bootdevice device
 * @bflowp: Returns a pointer to the bootflow
 * @return 0 if found, -ENOENT if there are no bootflows
 */
int bootdevice_first_bootflow(struct udevice *dev, struct bootflow **bflowp);

/**
 * bootdevice_next_bootflow() - Get the next bootflow from a bootdevice
 *
 * Returns the next bootflow attached to a bootdevice
 *
 * @bflowp: On entry, the last bootflow returned , e.g. from
 *	bootdevice_first_bootflow()
 * @return 0 if found, -ENOENT if there are no more bootflows
 */
int bootdevice_next_bootflow(struct bootflow **bflowp);

/**
 * bootdevice_setup_for_dev() - Bind a new bootdevice device
 *
 * Creates a bootdevice device as a child of @parent. This should be called from
 * the driver's bind() method or its uclass' post_bind() method.
 *
 * @parent: Parent device (e.g. MMC or Ethernet)
 * @drv_name: Name of bootdevice driver to bind
 * @return 0 if OK, -ve on error
 */
int bootdevice_setup_for_dev(struct udevice *parent, const char *drv_name);

#endif
