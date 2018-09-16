/*
 * Copyright (c) 2017 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <cros/fwstore.h>
#include <dm/device-internal.h>

int cros_fwstore_read(struct udevice *dev, uint32_t offset, uint32_t count,
		      void *buf)
{
	struct cros_fwstore_ops *ops = cros_fwstore_get_ops(dev);

	if (!ops->read)
		return -ENOSYS;

	return ops->read(dev, offset, count, buf);
}

int fwstore_get_reader_dev(struct udevice *fwstore, int offset, int size,
			   struct udevice **devp)
{
	struct udevice *dev;
	int ret;

	if (device_find_first_inactive_child(fwstore, UCLASS_MISC, &dev)) {
		ret = device_bind_ofnode(fwstore,
					 DM_GET_DRIVER(fwstore_reader),
					 "fwstore_reader", 0, ofnode_null(),
					 &dev);
		if (ret)
			return log_msg_ret("bind failed", ret);
	}
	fwstore_reader_setup(dev, offset, size);
	ret = device_probe(dev);
	if (ret)
		return ret;
	*devp = dev;

	return 0;
}

int fwstore_load_image(struct udevice *dev, int offset, int size,
		       enum fmap_compress_t compress_algo, int unc_size,
		       uint8_t **imagep, int *image_sizep)
{
	void *data, *buf;
	size_t buf_size;
	int ret;

	if (!size)
		return log_msg_ret("no image", -ENOENT);
	data = malloc(size);
	if (!data)
		return log_msg_ret("allocate space for image", -ENOMEM);
	ret = cros_fwstore_read(dev, offset, size, data);
	if (ret)
		return log_msg_ret("read image", ret);

	switch (compress_algo) {
	case FMAP_COMPRESS_NONE:
		*imagep = data;
		*image_sizep = size;
		break;
	case FMAP_COMPRESS_LZ4:
		buf_size = unc_size;
		buf = malloc(buf_size);
		if (!buf)
			return log_msg_ret("allocate decomp buf", -ENOMEM);
		ret = ulz4fn(data, size, buf, &buf_size);
		if (ret)
			return log_msg_ret("decompress lz4", ret);
		*imagep = buf;
		*image_sizep = buf_size;
		free(data);
		break;
	}

	return 0;
}

UCLASS_DRIVER(cros_fwstore) = {
	.id		= UCLASS_CROS_FWSTORE,
	.name		= "cros_fwstore",
};
