// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2017 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <rtc.h>
#include <cros/nvdata.h>

struct cmos_priv {
	u32 base_reg;
};

static int cmos_nvdata_read(struct udevice *dev, uint flags, uint8_t *data,
			       int size)
{
	struct cmos_priv *priv = dev_get_priv(dev);
	struct udevice *rtc = dev_get_parent(dev);
	int i, val;

	if (flags != CROS_NV_DATA) {
		log(UCLASS_CROS_NVDATA, LOGL_ERR,
		    "Only CROS_NV_DATA supported (not %x)\n", flags);
		return -ENOSYS;
	}

	for (i = 0; i < size; i++) {
		val = rtc_read8(rtc, priv->base_reg + i);
		if (val < 0)
			return log_msg_ret("Read CMOS RAM", val);
		data[i] = val;
	}

	return 0;
}

static int cmos_nvdata_write(struct udevice *dev, uint flags,
				const uint8_t *data, int size)
{
	struct cmos_priv *priv = dev_get_priv(dev);
	struct udevice *rtc = dev_get_parent(dev);
	int i, ret;

	if (flags != CROS_NV_DATA) {
		log(UCLASS_CROS_NVDATA, LOGL_ERR,
		    "Only CROS_NV_DATA supported (not %x)\n", flags);
		return -ENOSYS;
	}

	for (i = 0; i < size; i++) {
		ret = rtc_write8(rtc, priv->base_reg + i, data[i]);
		if (ret)
			return log_msg_ret("Write CMOS RAM", ret);
	}

	return 0;
}

static int cmos_nvdata_probe(struct udevice *dev)
{
	struct cmos_priv *priv = dev_get_priv(dev);
	int ret;

	ret = dev_read_u32(dev, "reg", &priv->base_reg);
	if (ret)
		return log_msg_ret("Missing 'reg' property", ret);

	return 0;
}

static const struct cros_nvdata_ops cmos_nvdata_ops = {
	.read	= cmos_nvdata_read,
	.write	= cmos_nvdata_write,
};

static const struct udevice_id cmos_nvdata_ids[] = {
	{ .compatible = "google,cmos-nvdata" },
	{ }
};

U_BOOT_DRIVER(cmos_nvdata_drv) = {
	.name		= "cmos-nvdata",
	.id		= UCLASS_CROS_NVDATA,
	.of_match	= cmos_nvdata_ids,
	.ops		= &cmos_nvdata_ops,
	.priv_auto_alloc_size	= sizeof(struct cmos_priv),
	.probe		= cmos_nvdata_probe,
};
