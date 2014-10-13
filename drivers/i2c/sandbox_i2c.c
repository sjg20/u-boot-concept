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

	*devp = NULL;
	*opsp = NULL;
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

static int sandbox_i2c_xfer(struct udevice *bus, struct i2c_msg *msg,
			    int nmsgs)
{
	struct dm_i2c_bus *i2c = bus->uclass_priv;
	struct dm_i2c_ops *ops;
	struct udevice *emul, *dev;
	struct dm_i2c_chip *chip;
	bool is_read;
	int ret;

	/* Special test code to return success but with no emulation */
	if (msg->addr == SANDBOX_I2C_TEST_ADDR)
		return 0;

	ret = get_emul(bus, msg->addr, &emul, &ops);
	if (ret)
		return ret;

	/* For testing, require an offset length of 1 */
	ret = i2c_get_chip(bus, msg->addr, &dev);
	if (ret)
		return ret;
	chip = dev_get_parentdata(dev);
	if (chip->offset_len != 1)
		return -EINVAL;

	/*
	 * For testing, don't allow writing above 100KHz for writes and
	 * 400KHz for reads
	 */
	is_read = nmsgs > 1;
	if (i2c->speed_hz > (is_read ? 400000 : 100000))
		return -EINVAL;
	return ops->xfer(emul, msg, nmsgs);
}

static int sandbox_i2c_set_offset_len(struct udevice *dev, uint offset_len)
{
	if (offset_len == 3)
		return -EINVAL;

	return 0;
}

static const struct dm_i2c_ops sandbox_i2c_ops = {
	.xfer		= sandbox_i2c_xfer,
	.set_offset_len	= sandbox_i2c_set_offset_len,
};

static int sandbox_i2c_child_pre_probe(struct udevice *dev)
{
	struct dm_i2c_chip *i2c_chip = dev_get_parentdata(dev);

	/* Ignore our test address */
	if (i2c_chip->chip_addr == SANDBOX_I2C_TEST_ADDR)
		return 0;
	if (dev->of_offset == -1)
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
