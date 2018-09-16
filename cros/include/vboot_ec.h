/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Chromium OS vboot EC uclass, used for vboot operations implemented by an EC
 *
 * Copyright 2018 Google, Inc
 */

#ifndef __CROS_VBOOT_EC_H
#define __CROS_VBOOT_EC_H

/**
 * struct vboot_ec_ops - EC operations required by vboot
 *
 * These directly correspond to normal vboot VbExEc... interfaces.
 */
struct vboot_ec_ops {
	int (*running_rw)(struct udevice *dev, int *in_rwp);
	int (*jump_to_rw)(struct udevice *dev);
	int (*disable_jump)(struct udevice *dev);
	int (*hash_image)(struct udevice *dev, enum VbSelectFirmware_t select,
			  const uint8_t **hashp, int *hash_sizep);
	int (*update_image)(struct udevice *dev, enum VbSelectFirmware_t select,
			    const uint8_t *image, int image_size);
	int (*protect)(struct udevice *dev, enum VbSelectFirmware_t select);
	int (*entering_mode)(struct udevice *dev, enum VbEcBootMode_t mode);

	/* Tells the EC to reboot to RO on next AP shutdown. */
	int (*reboot_to_ro)(struct udevice *dev);
};

#define vboot_ec_get_ops(dev)	((struct vboot_ec_ops *)(dev)->driver->ops)

int vboot_ec_running_rw(struct udevice *dev, int *in_rwp);
int vboot_ec_jump_to_rw(struct udevice *dev);
int vboot_ec_disable_jump(struct udevice *dev);
int vboot_ec_hash_image(struct udevice *dev, enum VbSelectFirmware_t select,
			const uint8_t **hashp, int *hash_sizep);
int vboot_ec_update_image(struct udevice *dev, enum VbSelectFirmware_t select,
			  const uint8_t *image, int image_size);
int vboot_ec_protect(struct udevice *dev, enum VbSelectFirmware_t select);
int vboot_ec_entering_mode(struct udevice *dev, enum VbEcBootMode_t mode);

/* Tells the EC to reboot to RO on next AP shutdown. */
int vboot_ec_reboot_to_ro(struct udevice *dev);

#endif /* __CROS_VBOOT_EC_H */
