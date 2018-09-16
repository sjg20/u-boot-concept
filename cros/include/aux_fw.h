/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Chromium OS alternative firmware, used to update firmware on devices in the
 * system other than those using UCLASS_VBOOT_EC.
 *
 * Copyright 2018 Google, Inc
 */

#ifndef __CROS_AUX_FW_H
#define __CROS_AUX_FW_H

enum aux_fw_severity {
	/* no update needed */
	AUX_FW_NO_UPDATE = 0,
	/* update needed, can be done quickly */
	AUX_FW_FAST_UPDATE = 1,
	/* update needed, "this would take a while..." */
	AUX_FW_SLOW_UPDATE = 2,
};

/**
 * struct aux_fw_ops - operations required by update process
 */
struct aux_fw_ops {
	int (*check_hash)(struct udevice *dev, const uint8_t *hash,
			  size_t hash_size, enum aux_fw_severity *severityp);

	/*
	 * @return -ERESTARTSYS to reboot to read-only firmware
	 */
	int (*update_image)(struct udevice *dev, const uint8_t *image,
			    size_t image_size);
	int (*protect)(struct udevice *dev);
};

#define aux_fw_get_ops(dev)	((struct aux_fw_ops *)(dev)->driver->ops)

int aux_fw_check_hash(struct udevice *dev, const uint8_t *hash,
		      size_t hash_size, enum aux_fw_severity *severityp);
int aux_fw_update_image(struct udevice *dev, const uint8_t *image,
			size_t image_size);
int aux_fw_protect(struct udevice *dev);

enum aux_fw_severity aux_fw_get_severity(struct udevice *dev);

#endif /* __CROS_AUX_FW_H */
