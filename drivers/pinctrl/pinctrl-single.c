// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) EETS GmbH, 2017, Felix Brack <f.brack@eets.ch>
 */

#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/devres.h>
#include <dm/of_access.h>
#include <dm/pinctrl.h>
#include <linux/libfdt.h>
#include <asm/io.h>

/**
 * struct single_gpiofunc_range - pin ranges with same mux value of gpio fun
 * @offset:	offset base of pins
 * @npins:	number pins with the same mux value of gpio function
 * @gpiofunc:	mux value of gpio function
 * @node:	list node
 */
struct single_gpiofunc_range {
	u32 offset;
	u32 npins;
	u32 gpiofunc;
	struct list_head node;
};

/**
 * struct single_pdata - pinctrl device instance
 * @base	first configuration register
 * @offset	index of last configuration register
 * @mask	configuration-value mask bits
 * @width	configuration register bit width
 * @bits_per_mux
 * @mutex	mutex protecting the list
 * @gpiofuncs	list of gpio functions
 * @read	register read function to use
 * @write	register write function to use
 */
struct single_pdata {
	fdt_addr_t base;	/* first configuration register */
	int offset;		/* index of last configuration register */
	u32 mask;		/* configuration-value mask bits */
	int width;		/* configuration register bit width */
	bool bits_per_mux;
	struct mutex mutex;
	struct list_head gpiofuncs;
	u32 (*read)(phys_addr_t reg);
	void (*write)(u32 val, phys_addr_t reg);
};

struct single_fdt_pin_cfg {
	fdt32_t reg;		/* configuration register offset */
	fdt32_t val;		/* configuration register value */
};

static u32 __maybe_unused single_readb(phys_addr_t reg)
{
	return readb(reg);
}

static u32 __maybe_unused single_readw(phys_addr_t reg)
{
	return readw(reg);
}

static u32 __maybe_unused single_readl(phys_addr_t reg)
{
	return readl(reg);
}

static void __maybe_unused single_writeb(u32 val, phys_addr_t reg)
{
	writeb(val, reg);
}

static void __maybe_unused single_writew(u32 val, phys_addr_t reg)
{
	writew(val, reg);
}

static void __maybe_unused single_writel(u32 val, phys_addr_t reg)
{
	writel(val, reg);
}

struct single_fdt_bits_cfg {
	fdt32_t reg;		/* configuration register offset */
	fdt32_t val;		/* configuration register value */
	fdt32_t mask;		/* configuration register mask */
};

/**
 * single_configure_pins() - Configure pins based on FDT data
 *
 * @dev: Pointer to single pin configuration device which is the parent of
 *       the pins node holding the pin configuration data.
 * @pins: Pointer to the first element of an array of register/value pairs
 *        of type 'struct single_fdt_pin_cfg'. Each such pair describes the
 *        the pin to be configured and the value to be used for configuration.
 *        This pointer points to a 'pinctrl-single,pins' property in the
 *        device-tree.
 * @size: Size of the 'pins' array in bytes.
 *        The number of register/value pairs in the 'pins' array therefore
 *        equals to 'size / sizeof(struct single_fdt_pin_cfg)'.
 */
static int single_configure_pins(struct udevice *dev,
				 const struct single_fdt_pin_cfg *pins,
				 int size)
{
	struct single_pdata *pdata = dev->platdata;
	int count = size / sizeof(struct single_fdt_pin_cfg);
	phys_addr_t n, reg;
	u32 val;

	for (n = 0; n < count; n++, pins++) {
		reg = fdt32_to_cpu(pins->reg);
		if ((reg < 0) || (reg > pdata->offset)) {
			dev_dbg(dev, "  invalid register offset 0x%pa\n", &reg);
			continue;
		}
		reg += pdata->base;
		val = fdt32_to_cpu(pins->val) & pdata->mask;
		val |= (pdata->read(reg) & ~pdata->mask);
		pdata->write(val, reg);

		dev_dbg(dev, "  reg/val 0x%pa/0x%08x\n", &reg, val);
	}
	return 0;
}

static int single_request(struct udevice *dev, int pin, int flags)
{
	struct single_pdata *pdata = dev->platdata;
	struct single_gpiofunc_range *frange = NULL;
	struct list_head *pos, *tmp;
	int mux_bytes = 0;
	u32 data;

	if (!pdata->mask)
		return -ENOTSUPP;

	list_for_each_safe(pos, tmp, &pdata->gpiofuncs) {
		frange = list_entry(pos, struct single_gpiofunc_range, node);
		if ((pin >= frange->offset + frange->npins) ||
		    pin < frange->offset)
			continue;

		mux_bytes = pdata->width / BITS_PER_BYTE;
		data = pdata->read(pdata->base + pin * mux_bytes);
		data &= ~pdata->mask;
		data |= frange->gpiofunc;
		pdata->write(data, pdata->base + pin * mux_bytes);
		break;
	}

	return 0;
}

static int single_configure_bits(struct udevice *dev,
				 const struct single_fdt_bits_cfg *pins,
				 int size)
{
	struct single_pdata *pdata = dev->platdata;
	int count = size / sizeof(struct single_fdt_bits_cfg);
	phys_addr_t n, reg;
	u32 val, mask;

	for (n = 0; n < count; n++, pins++) {
		reg = fdt32_to_cpu(pins->reg);
		if ((reg < 0) || (reg > pdata->offset)) {
			dev_dbg(dev, "  invalid register offset 0x%pa\n", &reg);
			continue;
		}
		reg += pdata->base;

		mask = fdt32_to_cpu(pins->mask);
		val = fdt32_to_cpu(pins->val) & mask;

		val |= (pdata->read(reg) & ~mask);
		pdata->write(val, reg);
		dev_dbg(dev, "  reg/val 0x%pa/0x%08x\n", &reg, val);
	}
	return 0;
}
static int single_set_state(struct udevice *dev,
			    struct udevice *config)
{
	const struct single_fdt_pin_cfg *prop;
	const struct single_fdt_bits_cfg *prop_bits;
	int len;

	prop = dev_read_prop(config, "pinctrl-single,pins", &len);

	if (prop) {
		dev_dbg(dev, "configuring pins for %s\n", config->name);
		if (len % sizeof(struct single_fdt_pin_cfg)) {
			dev_dbg(dev, "  invalid pin configuration in fdt\n");
			return -FDT_ERR_BADSTRUCTURE;
		}
		single_configure_pins(dev, prop, len);
		return 0;
	}

	/* pinctrl-single,pins not found so check for pinctrl-single,bits */
	prop_bits = dev_read_prop(config, "pinctrl-single,bits", &len);
	if (prop_bits) {
		dev_dbg(dev, "configuring pins for %s\n", config->name);
		if (len % sizeof(struct single_fdt_bits_cfg)) {
			dev_dbg(dev, "  invalid bits configuration in fdt\n");
			return -FDT_ERR_BADSTRUCTURE;
		}
		single_configure_bits(dev, prop_bits, len);
		return 0;
	}

	/* Neither 'pinctrl-single,pins' nor 'pinctrl-single,bits' were found */
	return len;
}

static int single_add_gpio_func(struct udevice *dev,
				struct single_pdata *pdata)
{
	const char *propname = "pinctrl-single,gpio-range";
	const char *cellname = "#pinctrl-single,gpio-range-cells";
	struct single_gpiofunc_range *range;
	struct ofnode_phandle_args gpiospec;
	int ret, i;

	for (i = 0; ; i++) {
		ret = ofnode_parse_phandle_with_args(dev->node, propname,
						     cellname, 0, i, &gpiospec);
		/* Do not treat it as error. Only treat it as end condition. */
		if (ret) {
			ret = 0;
			break;
		}
		range = devm_kzalloc(dev, sizeof(*range), GFP_KERNEL);
		if (!range) {
			ret = -ENOMEM;
			break;
		}
		range->offset = gpiospec.args[0];
		range->npins = gpiospec.args[1];
		range->gpiofunc = gpiospec.args[2];
		mutex_lock(&pdata->mutex);
		list_add_tail(&range->node, &pdata->gpiofuncs);
		mutex_unlock(&pdata->mutex);
	}
	return ret;
}

static int single_probe(struct udevice *dev)
{
	struct single_pdata *pdata = dev->platdata;
	int ret;

	switch (pdata->width) {
	case 8:
		pdata->read = single_readb;
		pdata->write = single_writeb;
		break;
	case 16:
		pdata->read = single_readw;
		pdata->write = single_writew;
		break;
	case 32:
		pdata->read = single_readl;
		pdata->write = single_writel;
		break;
	default:
		dev_warn(dev, "%s: unsupported register width %d\n",
			 __func__, pdata->width);
		return -EINVAL;
	}

	mutex_init(&pdata->mutex);
	INIT_LIST_HEAD(&pdata->gpiofuncs);

	ret = single_add_gpio_func(dev, pdata);
	if (ret < 0)
		dev_err(dev, "%s: Failed to add gpio functions\n", __func__);

	return ret;
}

static int single_ofdata_to_platdata(struct udevice *dev)
{
	fdt_addr_t addr;
	u32 of_reg[2];
	int res;
	struct single_pdata *pdata = dev->platdata;

	pdata->width =
		dev_read_u32_default(dev, "pinctrl-single,register-width", 0);

	res = dev_read_u32_array(dev, "reg", of_reg, 2);
	if (res)
		return res;
	pdata->offset = of_reg[1] - pdata->width / 8;

	addr = dev_read_addr(dev);
	if (addr == FDT_ADDR_T_NONE) {
		dev_dbg(dev, "no valid base register address\n");
		return -EINVAL;
	}
	pdata->base = addr;

	pdata->mask = dev_read_u32_default(dev, "pinctrl-single,function-mask",
					   0xffffffff);
	pdata->bits_per_mux = dev_read_bool(dev, "pinctrl-single,bit-per-mux");

	return 0;
}

const struct pinctrl_ops single_pinctrl_ops = {
	.set_state = single_set_state,
	.request = single_request,
};

static const struct udevice_id single_pinctrl_match[] = {
	{ .compatible = "pinctrl-single" },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(single_pinctrl) = {
	.name = "single-pinctrl",
	.id = UCLASS_PINCTRL,
	.of_match = single_pinctrl_match,
	.ops = &single_pinctrl_ops,
	.platdata_auto_alloc_size = sizeof(struct single_pdata),
	.ofdata_to_platdata = single_ofdata_to_platdata,
	.probe = single_probe,
};
