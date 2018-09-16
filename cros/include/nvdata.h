/*
 * Copyright (c) 2017 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __cros_nvdata_h
#define __cros_nvdata_h

enum cros_nvdata_index {
	CROS_NV_DATA,
	CROS_NV_SECDATA,
	CROS_NV_SECDATAK,
	CROS_NV_REC_HASH,
};

/* Operations for the Platform Controller Hub */
struct cros_nvdata_ops {
	/**
	 * read() - read non-volatile data
	 *
	 * @dev:	Device to read from
	 * @index:	Flags describing what to read (CROS_NV_...)
	 * @data:	Buffer for data read
	 * @len:	Length of data to read
	 * @return 0 if OK, -EMSGSIZE if the length does not match expectations,
	 *	other -ve value on other error
	 */
	int (*read)(struct udevice *dev, uint index, uint8_t *data, int size);

	int (*write)(struct udevice *dev, uint index, const uint8_t *data,
		     int size);
	int (*setup)(struct udevice *dev, uint index, uint attr,
		     const uint8_t *data, int size);
};

#define cros_nvdata_get_ops(dev) ((struct cros_nvdata_ops *)(dev)->driver->ops)

/**
 * cros_nvdata_read() - read non-volatile data
 *
 * @dev:	Device to read from
 * @index:	Flags describing what to read (CROS_NV_...)
 * @data:	Buffer for data read
 * @len:	Length of data to read
 * @return 0 if OK, -EMSGSIZE if the length does not match expectations,
 *	other -ve value on other error
 */
int cros_nvdata_read(struct udevice *dev, uint index, uint8_t *data, int size);

int cros_nvdata_write(struct udevice *dev, uint index, const uint8_t *data,
		      int size);
int cros_nvdata_setup(struct udevice *dev, uint index, uint attr,
		      const uint8_t *data, int size);

int cros_nvdata_read_walk(uint index, uint8_t *data, int size);
int cros_nvdata_write_walk(uint index, const uint8_t *data, int size);
int cros_nvdata_setup_walk(uint index, uint attr, const uint8_t *data,
			   int size);

#endif

