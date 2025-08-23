/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * HID over I2C interface for U-Boot
 *
 * Copyright (c) 2025
 */

#ifndef _HID_I2C_H
#define _HID_I2C_H

#if CONFIG_IS_ENABLED(HID_I2C)

/**
 * hid_i2c_init() - Initialize HID over I2C devices late in boot
 *
 * This function manually probes for HID over I2C devices after the display
 * is up and running, allowing debug output to be visible. It should be called
 * from main_loop() just before cli_loop().
 *
 * Return: 0 if successful, negative error code otherwise
 */
int hid_i2c_init(void);

#else

static inline int hid_i2c_init(void)
{
	return 0;
}

#endif /* CONFIG_IS_ENABLED(HID_I2C) */

#endif /* _HID_I2C_H */