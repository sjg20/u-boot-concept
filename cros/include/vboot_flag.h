/*
 * Copyright (c) 2017 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __cros_vboot_flag_h
#define __cros_vboot_flag_h

enum vboot_flag_t {
	VBOOT_FLAG_WRITE_PROTECT = 0,
	VBOOT_FLAG_DEVELOPER,
	VBOOT_FLAG_LID_OPEN,
	VBOOT_FLAG_POWER_OFF,
	VBOOT_FLAG_EC_IN_RW,
	VBOOT_FLAG_OPROM_LOADED,
	VBOOT_FLAG_RECOVERY,
	VBOOT_FLAG_WIPEOUT,

	VBOOT_FLAG_COUNT,
};

struct vboot_flag_details {
	/* previous value of the flag (1 or 0), or -1 if not known */
	int prev_value;
};

/* Operations for the verified boot flags */
struct vboot_flag_ops {
	/**
	 * read() - read non-volatile data
	 *
	 * @dev:	Device to read from
	 * @return flag value if OK (0 or 1), -ENOENT if not driver supports
	 *	the flag, -E2BIG if more than one driver supports the flag,
	 *	other -ve value on other error
	 */
	int (*read)(struct udevice *dev);
};

#define vboot_flag_get_ops(dev) ((struct vboot_flag_ops *)(dev)->driver->ops)

/**
 * vboot_flag_read() - read vboot flag
 *
 * @dev:	Device to read from
 * @return flag value if OK, -ENOENT if no driver supports the flag,
 *	-E2BIG if more than one driver supports the flag, other -ve
 *	value on other error
 */
int vboot_flag_read(struct udevice *dev);

int vboot_flag_read_walk(enum vboot_flag_t flag);

int vboot_flag_read_walk_prev(enum vboot_flag_t flag, int *prevp);

#endif
