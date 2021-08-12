/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __bootmethod_h
#define __bootmethod_h

#include <linux/list.h>

/**
 * enum bootflow_state_t - states that a particular bootflow can be in
 */
enum bootflow_state_t {
	BOOTFLOWST_BASE,	/**< Nothing known yet */
	BOOTFLOWST_MEDIA,	/**< Media exists */
	BOOTFLOWST_PART,	/**< Partition exists */
	BOOTFLOWST_FILE,	/**< Bootflow file exists */
	BOOTFLOWST_LOADED,	/**< Bootflow file loaded */

	BOOTFLOWST_COUNT
};

enum bootflow_type_t {
	BOOTFLOWT_DISTRO,	/**< Distro boot */

	BOOTFLOWT_COUNT,
};

/**
 * struct bootflow_state - information about available bootflows, etc.
 *
 * @cur_bootmethod: Currently selected bootmethod (for commands)
 */
struct bootflow_state {
	struct udevice *cur_bootmethod;
	struct bootflow *cur_bootflow;
};

/**
 * struct bootmethod_uc_priv - uclass information about a bootmethod
 *
 * @bootflows: List of available bootflows
 */
struct bootmethod_uc_priv {
	struct list_head bootflow_head;
};

extern struct bootflow_cmds g_bootflow_cmds;

/**
 * struct bootflow - information about a bootflow
 *
 * @seq: Sequence number of bootflow
 * @name: Name of bootflow
 * @type: Bootflow type (enum bootflow_type_t)
 * @state: Current state (enum bootflow_state_t)
 * @part: Partition number
 * @fname: Filename of bootflow file
 * @buf: Bootflow file contents
 */
struct bootflow {
	struct list_head sibling_node;
	int seq;
	char *name;
	enum bootflow_type_t type;
	enum bootflow_state_t state;
	int part;
	char *fname;
	char *buf;
};

struct bootmethod_iter {
	int flags;
	int seq;
	struct udevice *dev;
};

/**
 * enum bootflow_flags_t - flags for the bootflow
 *
 * @BOOTFLOWF_FIXED: Only used fixed/internal media
 */
enum bootflow_flags_t {
	BOOTFLOWF_FIXED		= 1 << 0,
};

/**
 * struct bootmethod_ops - Operations for the Platform Controller Hub
 *
 * Consider using ioctl() to add rarely used or driver-specific operations.
 */
struct bootmethod_ops {
	/**
	 * get_bootflow() - get a bootflow
	 *
	 * @dev:	Bootflow device to check
	 * @seq:	Sequence number of bootflow to read (0 for first)
	 * @bflow:	Returns bootflow if found
	 * @return sequence number of bootflow (>=0) if found, -ve on error
	 */
	int (*get_bootflow)(struct udevice *dev, int seq,
			    struct bootflow *bflow);
};

#define bootmethod_get_ops(dev)  ((struct bootmethod_ops *)(dev)->driver->ops)

/**
 * bootmethod_get_bootflow() - get a bootflow
 *
 * @dev:	Bootflow device to check
 * @seq:	Sequence number of bootflow to read (0 for first)
 * @bflow:	Returns bootflow if found
 * @return 0 if OK, -ve on error (e.g. there is no SPI base)
 */
int bootmethod_get_bootflow(struct udevice *dev, int seq,
			    struct bootflow *bflow);

/**
 * bootmethod_first_bootflow() - find the first bootflow
 *
 * This works through the available bootmethod devices until it finds one that
 * can supply a bootflow. It then returns that
 *
 * @iter:	Place to store private info (inited by this call)
 * @flags:	Flags for bootmethod (enum bootflow_flags_t)
 * @bflow:	Place to put the bootflow if found
 * @return 0 if found, -ESHUTDOWN if no more bootflows, other -ve on error
 */
int bootmethod_first_bootflow(struct bootmethod_iter *iter, int flags,
			      struct bootflow *bflow);

/**
 * bootmethod_next_bootflow() - find the next bootflow
 *
 * This works through the available bootmethod devices until it finds one that
 * can supply a bootflow. It then returns that bootflow
 *
 * @iter:	Private info (as set up by bootmethod_first_bootflow())
 * @bflow:	Place to put the bootflow if found
 * @return 0 if found, -ESHUTDOWN if no more bootflows, -ve on error
 */
int bootmethod_next_bootflow(struct bootmethod_iter *iter,
			     struct bootflow *bflow);

/**
 * bootmethod_bind() - Bind a new named bootmethod device
 *
 * @parent:	Parent of the new device
 * @drv_name:	Driver name to use for the bootmethod device
 * @name:	Name for the device (parent name is prepended)
 * @devp:	the new device (which has not been probed)
 */
int bootmethod_bind(struct udevice *parent, const char *drv_name,
		    const char *name, struct udevice **devp);

/**
 * bootmethod_find_in_blk() - Find a bootmethod in a block device
 *
 * @blk: Block device to search
 * @seq: Sequence number within block device, used as the partition number,
 *	after adding 1
 * @bflow:	Returns bootflow if found
 * @return 0 if found, -ESHUTDOWN if no more bootflows, other -ve on error
 */
int bootmethod_find_in_blk(struct udevice *blk, int seq,
			   struct bootflow *bflow);

/**
 * bootmethod_list() - List all available bootmethods
 *
 * @probe: true to probe devices, false to leave them as is
 */
void bootmethod_list(bool probe);

/**
 * bootmethod_state_get_name() - Get the name of a bootflow state
 *
 * @state: State to check
 * @return name, or "?" if invalid
 */
const char *bootmethod_state_get_name(enum bootflow_state_t state);

/**
 * bootmethod_type_get_name() - Get the name of a bootflow state
 *
 * @type: Type to check
 * @return name, or "?" if invalid
 */
const char *bootmethod_type_get_name(enum bootflow_type_t type);

int bootmethod_get_state(bool need_bootmethod, struct bootflow_state **statep);

void bootmethod_clear_bootflows(struct udevice *dev);

void bootmethod_add_bootflow(struct udevice *dev, struct bootflow *bflow);

#endif
