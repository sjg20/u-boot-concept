/*
 * Copyright (c) 2017 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <cros/nvdata.h>
#include <cros/vboot.h>

int cros_nvdata_read(struct udevice *dev, uint index, uint8_t *data, int size)
{
	struct cros_nvdata_ops *ops = cros_nvdata_get_ops(dev);

	if (!ops->read)
		return -ENOSYS;

	return ops->read(dev, index, data, size);
}

int cros_nvdata_read_walk(uint index, uint8_t *data, int size)
{
	struct udevice *dev;
	int ret = -ENOSYS;

	for (uclass_first_device(UCLASS_CROS_NVDATA, &dev);
	     dev;
	     uclass_next_device(&dev)) {
		ret = cros_nvdata_read(dev, index, data, size);
		if (!ret)
			break;
	}
	if (ret)
		return ret;

        return 0;
}

int cros_nvdata_write(struct udevice *dev, uint index, const uint8_t *data,
		      int size)
{
	struct cros_nvdata_ops *ops = cros_nvdata_get_ops(dev);

	if (!ops->write)
		return -ENOSYS;

	return ops->write(dev, index, data, size);
}

int cros_nvdata_setup(struct udevice *dev, uint index, uint attr,
		      const uint8_t *data, int size)
{
	struct cros_nvdata_ops *ops = cros_nvdata_get_ops(dev);

	if (!ops->setup)
		return -ENOSYS;

	return ops->setup(dev, index, attr, data, size);
}

int cros_nvdata_write_walk(uint index, const uint8_t *data, int size)
{
	struct udevice *dev;
	int ret = -ENOSYS;

	for (uclass_first_device(UCLASS_CROS_NVDATA, &dev);
	     dev;
	     uclass_next_device(&dev)) {
		log(UCLASS_CROS_NVDATA, LOGL_INFO, "write %s\n", dev->name);
		ret = cros_nvdata_write(dev, index, data, size);
		if (!ret)
			break;
	}
	if (ret)
		return ret;

        return 0;
}

int cros_nvdata_setup_walk(uint index, uint attr, const uint8_t *data,
			   int size)
{
	struct udevice *dev;
	int ret = -ENOSYS;

	for (uclass_first_device(UCLASS_CROS_NVDATA, &dev);
	     dev;
	     uclass_next_device(&dev)) {
		ret = cros_nvdata_setup(dev, index, attr, data, size);
		if (!ret)
			break;
	}
	if (ret)
		return ret;

        return 0;
}

VbError_t VbExNvStorageRead(uint8_t* buf)
{
	int ret;

	ret = cros_nvdata_read_walk(CROS_NV_DATA, buf, EC_VBNV_BLOCK_SIZE);
	if (ret)
		return VBERROR_UNKNOWN;
	print_buffer(0, buf, 1, EC_VBNV_BLOCK_SIZE, 0);

	return 0;
}

VbError_t VbExNvStorageWrite(const uint8_t* buf)
{
	int ret;

	printf("write\n");
	print_buffer(0, buf, 1, EC_VBNV_BLOCK_SIZE, 0);
	ret = cros_nvdata_write_walk(CROS_NV_DATA, buf, EC_VBNV_BLOCK_SIZE);
	if (ret)
		return VBERROR_UNKNOWN;

	return 0;
}

UCLASS_DRIVER(cros_nvdata) = {
	.id		= UCLASS_CROS_NVDATA,
	.name		= "cros_nvdata",
};
