// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2017 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <cros_ec.h>
#include <cros/nvdata.h>

static int cros_ec_nvdata_read(struct udevice *dev, uint flags, uint8_t *data,
			       int size)
{
	struct udevice *cros_ec = dev_get_parent(dev);

	if (flags != CROS_NV_DATA) {
		log(UCLASS_CROS_NVDATA, LOGL_ERR,
		    "Only CROS_NV_DATA supported (not %x)\n", flags);
		return -ENOSYS;
	}

	return cros_ec_read_nvdata(cros_ec, data, size);
}

static int cros_ec_nvdata_write(struct udevice *dev, uint flags,
				const uint8_t *data, int size)
{
	struct udevice *cros_ec = dev_get_parent(dev);

	if (flags != CROS_NV_DATA) {
		log(UCLASS_CROS_NVDATA, LOGL_ERR,
		    "Only CROS_NV_DATA supported (not %x)\n", flags);
		return -ENOSYS;
	}

	return cros_ec_write_nvdata(cros_ec, data, size);
}

static const struct cros_nvdata_ops cros_ec_nvdata_ops = {
	.read	= cros_ec_nvdata_read,
	.write	= cros_ec_nvdata_write,
};

static const struct udevice_id cros_ec_nvdata_ids[] = {
	{ .compatible = "google,cros-ec-nvdata" },
	{ }
};

U_BOOT_DRIVER(cros_ec_nvdata_drv) = {
	.name		= "cros-ec-nvdata",
	.id		= UCLASS_CROS_NVDATA,
	.of_match	= cros_ec_nvdata_ids,
	.ops		= &cros_ec_nvdata_ops,
};
