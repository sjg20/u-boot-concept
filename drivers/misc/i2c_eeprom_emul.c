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

static int sandbox_i2c_eprom_probe_chip(struct udevice *dev, uint chip)
{
	return 0;
}

static int sandbox_i2c_eprom_read(struct udevice *dev, uint chip_addr,
				  uint addr, uint alen, uint8_t *buffer,
				  int len)
{
	struct sandbox_i2c_flash_plat_data *plat = dev_get_platdata(dev);
	struct sandbox_i2c_flash *priv = dev_get_priv(dev);

	if (addr + len > plat->size)
		return -EINVAL;
	memcpy(buffer, priv->data + addr, len);

	return 0;
}

static int sandbox_i2c_eprom_write(struct udevice *dev, uint chip_addr,
				   uint addr, uint alen, const uint8_t *buffer,
				   int len)
{
	struct sandbox_i2c_flash_plat_data *plat = dev_get_platdata(dev);
	struct sandbox_i2c_flash *priv = dev_get_priv(dev);

	if (addr + len > plat->size)
		return -EINVAL;
	memcpy(priv->data + addr, buffer, len);

	return 0;
}

struct dm_i2c_ops sandbox_i2c_emul_ops = {
	.probe = sandbox_i2c_eprom_probe_chip,
	.read = sandbox_i2c_eprom_read,
	.write = sandbox_i2c_eprom_write,
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
