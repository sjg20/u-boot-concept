/*
 * Copyright (c) 2014 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <fdtdec.h>
#include <i2c.h>
#include <malloc.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/root.h>

DECLARE_GLOBAL_DATA_PTR;

static bool i2c_setup_offset(struct dm_i2c_chip *chip, uint offset,
			     uint8_t offset_buf[], struct i2c_msg *msg)
{
	if (!chip->offset_len)
		return false;
	msg->addr = chip->chip_addr;
	msg->flags = chip->flags;
	msg->len = chip->offset_len;
	msg->buf = offset_buf;
	offset_buf[0] = offset;
	offset_buf[1] = offset >> 8;
	offset_buf[2] = offset >> 16;
	offset_buf[3] = offset >> 24;

	return true;
}

static int i2c_read_bytewise(struct udevice *dev, uint offset,
			     const uint8_t *buffer, int len)
{
	struct dm_i2c_chip *chip = dev_get_parentdata(dev);
	struct udevice *bus = dev_get_parent(dev);
	struct dm_i2c_ops *ops = i2c_get_ops(bus);
	struct i2c_msg msg[1];
	uint8_t buf[5];
	int ret;
	int i;

	for (i = 0; i < len; i++) {
		i2c_setup_offset(chip, offset, buf, msg);
		msg->len++;
		buf[chip->offset_len] = buffer[i];

		ret = ops->xfer(bus, msg, 1);
		if (ret)
			return ret;
	}

	return 0;
}

int i2c_read(struct udevice *dev, uint offset, uint8_t *buffer, int len)
{
	struct dm_i2c_chip *chip = dev_get_parentdata(dev);
	struct udevice *bus = dev_get_parent(dev);
	struct dm_i2c_ops *ops = i2c_get_ops(bus);
	struct i2c_msg msg[2], *ptr;
	uint8_t offset_buf[4];
	int msg_count;

	if (!ops->xfer)
		return -ENOSYS;
	if (chip->flags & DM_I2C_CHIP_RE_ADDRESS)
		return i2c_read_bytewise(dev, offset, buffer, len);
	ptr = msg;
	if (i2c_setup_offset(chip, offset, offset_buf, ptr))
		ptr++;

	if (len) {
		ptr->addr = chip->chip_addr;
		ptr->flags = chip->flags | I2C_M_RD;
		ptr->len = len;
		ptr->buf = buffer;
		ptr++;
	}
	msg_count = ptr - msg;

	return ops->xfer(bus, msg, msg_count);
}

int i2c_write(struct udevice *dev, uint offset, const uint8_t *buffer, int len)
{
	struct dm_i2c_chip *chip = dev_get_parentdata(dev);
	struct udevice *bus = dev_get_parent(dev);
	struct dm_i2c_ops *ops = i2c_get_ops(bus);
	struct i2c_msg msg[1];

	if (!ops->xfer)
		return -ENOSYS;

	/*
	 * The simple approach would be to send two messages here: one to
	 * set the offset and one to write the bytes. However some drivers
	 * will not be expecting this, and some chips won't like how the
	 * driver presents this on the I2C bus.
	 *
	 * The API does not support separate offset and data. We could extend
	 * it with a flag indicating that there is data in the next message
	 * that needs to be processed in the same transaction. We could
	 * instead add an additional buffer to each message. For now, handle
	 * this in the uclass since it isn't clear what the impact on drivers
	 * would be with this extra complication. Unfortunately this means
	 * copying the message.
	 *
	 * Use the stack for small messages, malloc() for larger ones. We
	 * need to allow space for the offset (up to 4 bytes) and the message
	 * itself.
	 */
	if (len < 64) {
		uint8_t buf[4 + len];

		i2c_setup_offset(chip, offset, buf, msg);
		msg->len += len;
		memcpy(buf + chip->offset_len, buffer, len);

		return ops->xfer(bus, msg, 1);
	} else {
		uint8_t *buf;
		int ret;

		buf = malloc(4 + len);
		if (!buf)
			return -ENOMEM;
		i2c_setup_offset(chip, offset, buf, msg);
		msg->len += len;
		memcpy(buf + chip->offset_len, buffer, len);

		ret = ops->xfer(bus, msg, 1);
		free(buf);
		return ret;
	}
}

static int i2c_probe_chip(struct udevice *bus, uint chip_addr)
{
	struct dm_i2c_ops *ops = i2c_get_ops(bus);
	struct i2c_msg msg[1];
	uint8_t ch = 0;

	if (!ops->xfer)
		return -ENOSYS;

	msg->addr = chip_addr;
	msg->flags = 0;
	msg->len = 1;
	msg->buf = &ch;

	return ops->xfer(bus, msg, 1);
}

static int i2c_bind_driver(struct udevice *bus, uint chip_addr,
			   struct udevice **devp)
{
	struct dm_i2c_chip *chip;
	char name[30], *str;
	struct udevice *dev;
	int ret;

	snprintf(name, sizeof(name), "generic_%x", chip_addr);
	str = strdup(name);
	ret = device_bind_driver(bus, "i2c_generic_drv", str, &dev);
	debug("%s:  device_bind_driver: ret=%d\n", __func__, ret);
	if (ret)
		goto err_bind;

	/* Tell the device what we know about it */
	chip = calloc(1, sizeof(struct dm_i2c_chip));
	if (!chip) {
		ret = -ENOMEM;
		goto err_mem;
	}
	chip->chip_addr = chip_addr;
	chip->offset_len = 1;	/* we assume */
	ret = device_probe_child(dev, chip);
	debug("%s:  device_probe_child: ret=%d\n", __func__, ret);
	free(chip);
	if (ret)
		goto err_probe;

	*devp = dev;
	return 0;

err_probe:
err_mem:
	device_unbind(dev);
err_bind:
	free(str);
	return ret;
}

int i2c_get_chip(struct udevice *bus, uint chip_addr, struct udevice **devp)
{
	struct udevice *dev;

	debug("%s: Searching bus '%s' for address %02x: ", __func__,
	      bus->name, chip_addr);
	for (device_find_first_child(bus, &dev); dev;
			device_find_next_child(&dev)) {
		struct dm_i2c_chip store;
		struct dm_i2c_chip *chip = dev_get_parentdata(dev);
		int ret;

		if (!chip) {
			chip = &store;
			i2c_chip_ofdata_to_platdata(gd->fdt_blob,
						    dev->of_offset, chip);
		}
		if (chip->chip_addr == chip_addr) {
			ret = device_probe(dev);
			debug("found, ret=%d\n", ret);
			if (ret)
				return ret;
			*devp = dev;
			return 0;
		}
	}
	debug("not found\n");
	return i2c_bind_driver(bus, chip_addr, devp);
}

int i2c_get_chip_for_busnum(int busnum, int chip_addr, struct udevice **devp)
{
	struct udevice *bus;
	int ret;

	ret = uclass_get_device_by_seq(UCLASS_I2C, busnum, &bus);
	if (ret) {
		debug("Cannot find I2C bus %d\n", busnum);
		return ret;
	}
	ret = i2c_get_chip(bus, chip_addr, devp);
	if (ret) {
		debug("Cannot find I2C chip %02x on bus %d\n", chip_addr,
		      busnum);
		return ret;
	}

	return 0;
}

int i2c_probe(struct udevice *bus, uint chip_addr, struct udevice **devp)
{
	int ret;

	*devp = NULL;

	/* First probe that chip */
	ret = i2c_probe_chip(bus, chip_addr);
	debug("%s: bus='%s', address %02x, ret=%d\n", __func__, bus->name,
	      chip_addr, ret);
	if (ret)
		return ret;

	/* The chip was found, see if we have a driver, and probe it */
	ret = i2c_get_chip(bus, chip_addr, devp);
	debug("%s:  i2c_get_chip: ret=%d\n", __func__, ret);

	return ret;
}

int i2c_set_bus_speed(struct udevice *bus, unsigned int speed)
{
	struct dm_i2c_ops *ops = i2c_get_ops(bus);
	struct dm_i2c_bus *i2c = bus->uclass_priv;
	int ret;

	if (ops->set_bus_speed) {
		ret = ops->set_bus_speed(bus, speed);
		if (ret)
			return ret;
	}
	i2c->speed_hz = speed;

	return 0;
}

/*
 * i2c_get_bus_speed:
 *
 *  Returns speed of selected I2C bus in Hz
 */
int i2c_get_bus_speed(struct udevice *bus)
{
	struct dm_i2c_ops *ops = i2c_get_ops(bus);
	struct dm_i2c_bus *i2c = bus->uclass_priv;

	if (!ops->set_bus_speed)
		return i2c->speed_hz;

	return ops->get_bus_speed(bus);
}

int i2c_set_chip_flags(struct udevice *dev, uint flags)
{
	struct udevice *bus = dev->parent;
	struct dm_i2c_chip *chip = dev_get_parentdata(dev);
	struct dm_i2c_ops *ops = i2c_get_ops(bus);
	int ret;

	if (ops->set_flags) {
		ret = ops->set_flags(dev, flags);
		if (ret)
			return ret;
	}
	chip->flags = flags;

	return 0;
}

int i2c_get_chip_flags(struct udevice *dev, uint *flagsp)
{
	struct dm_i2c_chip *chip = dev_get_parentdata(dev);

	*flagsp = chip->flags;

	return 0;
}

int i2c_set_chip_offset_len(struct udevice *dev, uint offset_len)
{
	struct udevice *bus = dev->parent;
	struct dm_i2c_chip *chip = dev_get_parentdata(dev);
	struct dm_i2c_ops *ops = i2c_get_ops(bus);
	int ret;

	if (offset_len > 3)
		return -EINVAL;
	if (ops->set_offset_len) {
		ret = ops->set_offset_len(dev, offset_len);
		if (ret)
			return ret;
	}
	chip->offset_len = offset_len;

	return 0;
}

int i2c_deblock(struct udevice *bus)
{
	struct dm_i2c_ops *ops = i2c_get_ops(bus);

	/*
	 * We could implement a software deblocking here if we could get
	 * access to the GPIOs used by I2C, and switch them to GPIO mode
	 * and then back to I2C. This is somewhat beyond our powers in
	 * driver model at present, so for now just fail.
	 *
	 * See https://patchwork.ozlabs.org/patch/399040/
	 */
	if (!ops->deblock)
		return -ENOSYS;

	return ops->deblock(bus);
}

int i2c_chip_ofdata_to_platdata(const void *blob, int node,
				struct dm_i2c_chip *chip)
{
	chip->offset_len = 1;	/* default */
	chip->flags = 0;
	chip->chip_addr = fdtdec_get_int(gd->fdt_blob, node, "reg", -1);
	if (chip->chip_addr == -1) {
		debug("%s: I2C Node '%s' has no 'reg' property\n", __func__,
		      fdt_get_name(blob, node, NULL));
		return -EINVAL;
	}

	return 0;
}

static int i2c_post_probe(struct udevice *dev)
{
	struct dm_i2c_bus *i2c = dev->uclass_priv;

	i2c->speed_hz = fdtdec_get_int(gd->fdt_blob, dev->of_offset,
				     "clock-frequency", 100000);

	return i2c_set_bus_speed(dev, i2c->speed_hz);
}

int i2c_post_bind(struct udevice *dev)
{
	/* Scan the bus for devices */
	return dm_scan_fdt_node(dev, gd->fdt_blob, dev->of_offset, false);
}

UCLASS_DRIVER(i2c) = {
	.id		= UCLASS_I2C,
	.name		= "i2c",
	.per_device_auto_alloc_size = sizeof(struct dm_i2c_bus),
	.post_bind	= i2c_post_bind,
	.post_probe	= i2c_post_probe,
};

UCLASS_DRIVER(i2c_generic) = {
	.id		= UCLASS_I2C_GENERIC,
	.name		= "i2c_generic",
};

U_BOOT_DRIVER(i2c_generic_drv) = {
	.name		= "i2c_generic_drv",
	.id		= UCLASS_I2C_GENERIC,
};
