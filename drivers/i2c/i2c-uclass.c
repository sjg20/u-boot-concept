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

int i2c_read(struct udevice *dev, uint addr, uint8_t *buffer, int len)
{
	struct dm_i2c_chip *chip = dev_get_parentdata(dev);
	struct udevice *bus = dev_get_parent(dev);
	struct dm_i2c_ops *ops = i2c_get_ops(bus);

	if (!ops->read)
		return -ENOSYS;

	return ops->read(bus, chip->chip_addr, addr, chip->addr_len, buffer,
			 len);
}

int i2c_write(struct udevice *dev, uint addr, const uint8_t *buffer, int len)
{
	struct dm_i2c_chip *chip = dev_get_parentdata(dev);
	struct udevice *bus = dev_get_parent(dev);
	struct dm_i2c_ops *ops = i2c_get_ops(bus);

	if (!ops->write)
		return -ENOSYS;

	return ops->write(bus, chip->chip_addr, addr, chip->addr_len, buffer,
			  len);
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
	chip->addr_len = 1;	/* we assume */
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
	struct dm_i2c_ops *ops = i2c_get_ops(bus);
	int ret;

	*devp = NULL;
	if (!ops->probe)
		return -ENODEV;

	/* First probe that chip */
	ret = ops->probe(bus, chip_addr);
	debug("%s: bus='%s', address %02x, ret=%d\n", __func__, bus->name,
	      chip_addr, ret);
	if (ret)
		return ret;

	/* The chip was found, see if we have a driver, and probe it */
	ret = i2c_get_chip(bus, chip_addr, devp);
	debug("%s:  i2c_get_chip: ret=%d\n", __func__, ret);
	if (!ret || ret != -ENODEV)
		return ret;

	return i2c_bind_driver(bus, chip_addr, devp);
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

int i2c_set_addr_len(struct udevice *dev, uint addr_len)
{
	struct udevice *bus = dev->parent;
	struct dm_i2c_chip *chip = dev_get_parentdata(dev);
	struct dm_i2c_ops *ops = i2c_get_ops(bus);
	int ret;

	if (addr_len > 3)
		return -EINVAL;
	if (ops->set_addr_len) {
		ret = ops->set_addr_len(dev, addr_len);
		if (ret)
			return ret;
	}
	chip->addr_len = addr_len;

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
	chip->addr_len = 1;	/* default */
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
