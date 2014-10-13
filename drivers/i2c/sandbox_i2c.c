/*
 * Simulate an I2C port
 *
 * Copyright (c) 2014 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <fdtdec.h>
#include <i2c.h>
#include <asm/test.h>
#include <dm/lists.h>
#include <dm/device-internal.h>

DECLARE_GLOBAL_DATA_PTR;

struct dm_sandbox_i2c_emul_priv {
	struct udevice *emul;
};

static int get_emul(struct udevice *bus, uint chip_addr, struct udevice **devp,
		    struct dm_i2c_ops **opsp)
{
	const void *blob = gd->fdt_blob;
	struct dm_i2c_chip *priv;
	struct udevice *dev;
	int ret;

	ret = i2c_get_chip(bus, chip_addr, &dev);
	if (ret)
		return ret;
	priv = dev_get_parentdata(dev);
	if (!priv->emul) {
		int node;

		debug("Scanning i2c bus '%s' for devices\n", dev->name);
		for (node = fdt_first_subnode(blob, dev->of_offset);
			node >= 0;
			node = fdt_next_subnode(blob, node)) {
			int ret;

			ret = lists_bind_fdt(dev, blob, node, &priv->emul);
			if (ret)
				return ret;
			debug("Found emul '%s' for i2c device '%s'\n",
			      priv->emul->name, dev->name);
			break;
		}
	}

	if (!priv->emul)
		return -ENODEV;
	ret = device_probe(priv->emul);
	if (ret)
		return ret;
	*devp = priv->emul;
	*opsp = i2c_get_ops(priv->emul);

	return 0;
}

static int sandbox_i2c_probe_chip(struct udevice *bus, uint chip_addr)
{
	struct dm_i2c_ops *ops;
	struct udevice *emul;
	int ret;

	/* Special test code to return success but with no emulation */
	if (chip_addr == SANDBOX_I2C_TEST_ADDR)
		return 0;

	ret = get_emul(bus, chip_addr, &emul, &ops);
	if (ret)
		return ret;

	return ops->probe(emul, chip_addr);
}

static int sandbox_i2c_read(struct udevice *bus, uint chip_addr, uint addr,
			    uint alen, uint8_t *buffer, int len)
{
	struct dm_i2c_bus *i2c = bus->uclass_priv;
	struct dm_i2c_ops *ops;
	struct udevice *emul;
	int ret;

	ret = get_emul(bus, chip_addr, &emul, &ops);
	if (ret)
		return ret;

	/* For testing, don't allow reading above 400KHz */
	if (i2c->speed_hz > 400000 || alen != 1)
		return -EINVAL;
	return ops->read(emul, chip_addr, addr, alen, buffer, len);
}

static int sandbox_i2c_write(struct udevice *bus, uint chip_addr, uint addr,
			     uint alen, const uint8_t *buffer, int len)
{
	struct dm_i2c_bus *i2c = bus->uclass_priv;
	struct dm_i2c_ops *ops;
	struct udevice *emul;
	int ret;

	ret = get_emul(bus, chip_addr, &emul, &ops);
	if (ret)
		return ret;

	/* For testing, don't allow writing above 100KHz */
	if (i2c->speed_hz > 100000 || alen != 1)
		return -EINVAL;
	return ops->write(emul, chip_addr, addr, alen, buffer, len);
}

static int sandbox_i2c_set_addr_len(struct udevice *dev, uint addr_len)
{
	if (addr_len == 3)
		return -EINVAL;

	return 0;
}

static const struct dm_i2c_ops sandbox_i2c_ops = {
	.probe		= sandbox_i2c_probe_chip,
	.read		= sandbox_i2c_read,
	.write		= sandbox_i2c_write,
	.set_addr_len	= sandbox_i2c_set_addr_len,
};

static int sandbox_i2c_child_pre_probe(struct udevice *dev)
{
	struct dm_i2c_chip *i2c_chip = dev_get_parentdata(dev);

	/* Ignore our test address */
	if (i2c_chip->chip_addr == SANDBOX_I2C_TEST_ADDR)
		return 0;
	return i2c_chip_ofdata_to_platdata(gd->fdt_blob, dev->of_offset,
					   i2c_chip);
}

static const struct udevice_id sandbox_i2c_ids[] = {
	{ .compatible = "sandbox,i2c" },
	{ }
};

U_BOOT_DRIVER(i2c_sandbox) = {
	.name	= "i2c_sandbox",
	.id	= UCLASS_I2C,
	.of_match = sandbox_i2c_ids,
	.per_child_auto_alloc_size = sizeof(struct dm_i2c_chip),
	.child_pre_probe = sandbox_i2c_child_pre_probe,
	.ops	= &sandbox_i2c_ops,
};
