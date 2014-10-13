/*
 * Simulate an I2C eeprom
 *
 * Copyright (c) 2014 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <fdtdec.h>
#include <i2c.h>
#include <malloc.h>

DECLARE_GLOBAL_DATA_PTR;

struct sandbox_i2c_flash_plat_data {
	const char *filename;
	int size;
};

struct sandbox_i2c_flash {
	uint8_t *data;
};

static int sandbox_i2c_eprom_xfer(struct udevice *emul, struct i2c_msg *msg,
				  int nmsgs)
{
	int offset = 0;

	/* Always let probe succeed */
	if (nmsgs == 1 && msg->len == 1) {
		printf("%s: Detected probe\n", __func__);
		return 0;
	}

	for (; nmsgs > 0; nmsgs--, msg++) {
		struct sandbox_i2c_flash_plat_data *plat =
				dev_get_platdata(emul);
		struct sandbox_i2c_flash *priv = dev_get_priv(emul);

		if (!plat->size)
			return -ENODEV;
		if (msg->addr + msg->len > plat->size) {
			debug("%s: Address %x, len %x is outside range 0..%x\n",
			      __func__, msg->addr, msg->len, plat->size);
			return -EINVAL;
		}
		if (msg->flags & I2C_M_RD) {
			memcpy(msg->buf, priv->data + offset, msg->len);
		} else {
			offset = *msg->buf;
			memcpy(priv->data + offset, msg->buf + 1,
			       msg->len - 1);
		}
	}

	return 0;
}

struct dm_i2c_ops sandbox_i2c_emul_ops = {
	.xfer = sandbox_i2c_eprom_xfer,
};

static int sandbox_i2c_eeprom_ofdata_to_platdata(struct udevice *dev)
{
	struct sandbox_i2c_flash_plat_data *plat = dev_get_platdata(dev);

	plat->size = fdtdec_get_int(gd->fdt_blob, dev->of_offset,
				    "sandbox,size", 32);
	plat->filename = fdt_getprop(gd->fdt_blob, dev->of_offset,
				     "sandbox,filename", NULL);
	if (!plat->filename) {
		debug("%s: No filename for device '%s'\n", __func__,
		      dev->name);
		return -EINVAL;
	}

	return 0;
}

static int sandbox_i2c_eeprom_probe(struct udevice *dev)
{
	struct sandbox_i2c_flash_plat_data *plat = dev_get_platdata(dev);
	struct sandbox_i2c_flash *priv = dev_get_priv(dev);

	priv->data = calloc(1, plat->size);
	if (!priv->data)
		return -ENOMEM;

	return 0;
}

static const struct udevice_id sandbox_i2c_ids[] = {
	{ .compatible = "sandbox,i2c-eeprom" },
	{ }
};

U_BOOT_DRIVER(sandbox_i2c_emul) = {
	.name		= "sandbox_i2c_eeprom_emul",
	.id		= UCLASS_I2C_EMUL,
	.of_match	= sandbox_i2c_ids,
	.ofdata_to_platdata = sandbox_i2c_eeprom_ofdata_to_platdata,
	.probe		= sandbox_i2c_eeprom_probe,
	.priv_auto_alloc_size = sizeof(struct sandbox_i2c_flash),
	.platdata_auto_alloc_size = sizeof(struct sandbox_i2c_flash_plat_data),
	.ops		= &sandbox_i2c_emul_ops,
};
