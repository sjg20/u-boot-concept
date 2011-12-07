/*
 * Copyright (c) 2011 The Chromium OS Authors.
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>

#include <fdt_decode.h>
#include <libfdt.h>
#include <malloc.h>
#include <serial.h>

/* we need a generic GPIO interface here */
#include <asm/arch/gpio.h>

/*
 * Here are the type we know about. One day we might allow drivers to
 * register. For now we just put them here. The COMPAT macro allows us to
 * turn this into a sparse list later, and keeps the ID with the name.
 */
#define COMPAT(id, name) name
static const char *compat_names[COMPAT_COUNT] = {
	COMPAT(UNKNOWN, "<none>"),
	COMPAT(NVIDIA_SPI_UART_SWITCH, "nvidia,spi-uart-switch"),
	COMPAT(SERIAL_NS16550, "ns16550"),
	COMPAT(NVIDIA_TEGRA250_USB, "nvidia,tegra250-usb"),
	COMPAT(NVIDIA_TEGRA250_SDHCI, "nvidia,tegra250-sdhci"),
	COMPAT(NVIDIA_TEGRA250_KBC, "nvidia,tegra250-kbc"),
	COMPAT(NVIDIA_TEGRA250_I2C, "nvidia,tegra250-i2c"),
};

/**
 * Look in the FDT for an alias with the given name and return its node.
 *
 * @param blob	FDT blob
 * @param name	alias name to look up
 * @return node offset if found, or an error code < 0 otherwise
 */
static int find_alias_node(const void *blob, const char *name)
{
	const char *path;
	int alias_node;

	debug("%s: %s\n", __func__, name);
	alias_node = fdt_path_offset(blob, "/aliases");
	if (alias_node < 0)
		return alias_node;
	path = fdt_getprop(blob, alias_node, name, NULL);
	if (!path)
		return -FDT_ERR_NOTFOUND;
	return fdt_path_offset(blob, path);
}

/**
 * Look up an address property in a node and return it as an address.
 * The property must hold either one address with no trailing data or
 * one address with a length. This is only tested on 32-bit machines.
 *
 * @param blob	FDT blob
 * @param node	node to examine
 * @param prop_name	name of property to find
 * @return address, if found, or ADDR_T_NONE if not
 */
static addr_t get_addr(const void *blob, int node, const char *prop_name)
{
	const addr_t *cell;
	int len;

	debug("%s: %s\n", __func__, prop_name);
	cell = fdt_getprop(blob, node, prop_name, &len);
	if (cell && (len == sizeof(addr_t) || len == sizeof(addr_t) * 2))
		return addr_to_cpu(*cell);
	return ADDR_T_NONE;
}

/**
 * Look up a 32-bit integer property in a node and return it. The property
 * must have at least 4 bytes of data. The value of the first cell is
 * returned.
 *
 * @param blob	FDT blob
 * @param node	node to examine
 * @param prop_name	name of property to find
 * @param default_val	default value to return if the property is not found
 * @return integer value, if found, or default_val if not
 */
static s32 get_int(const void *blob, int node, const char *prop_name,
		s32 default_val)
{
	const s32 *cell;
	int len;

	debug("%s: %s\n", __func__, prop_name);
	cell = fdt_getprop(blob, node, prop_name, &len);
	if (cell && len >= sizeof(s32))
		return fdt32_to_cpu(cell[0]);
	return default_val;
}

/**
 * Look up a boolean property in a node and return it.
 *
 * A boolean properly is true if present in the device tree and false if not
 * present.
 *
 * @param blob	FDT blob
 * @param node	node to examine
 * @param prop_name	name of property to find
 * @return 1 if the properly is present; 0 if it isn't present
 */
static int get_bool(const void *blob, int node, const char *prop_name)
{
	const s32 *cell;
	int len;

	debug("%s: %s\n", __func__, prop_name);
	cell = fdt_getprop(blob, node, prop_name, &len);
	return (cell) ? 1 : 0;
}

/**
 * Look up a property in a node and check that it has a minimum length.
 *
 * @param blob		FDT blob
 * @param node		node to examine
 * @param prop_name	name of property to find
 * @param min_len	minimum property length in bytes
 * @param err		0 if ok, or -FDT_ERR_MISSING if the property is not
			found, or -FDT_ERR_BADLAYOUT if not enough data
 * @return pointer to cell, which is only valid if err == 0
 */
static const void *get_prop_len(const void *blob, int node,
		const char *prop_name, int min_len, int *err)
{
	const void *cell;
	int len;

	debug("%s: %s\n", __func__, prop_name);
	cell = fdt_getprop(blob, node, prop_name, &len);
	if (!cell)
		*err = -FDT_ERR_MISSING;
	else if (len < min_len)
		*err = -FDT_ERR_BADLAYOUT;
	else
		*err = 0;
	return cell;
}

/**
 * Look up a property in a node and return its contents in an integer
 * array of given length. The property must have at least enough data for
 * the array (4*count bytes). It may have more, but this will be ignored.
 *
 * @param blob		FDT blob
 * @param node		node to examine
 * @param prop_name	name of property to find
 * @param array		array to fill with data
 * @param count		number of array elements
 * @return 0 if ok, or -FDT_ERR_MISSING if the property is not found,
 *		or -FDT_ERR_BADLAYOUT if not enough data
 */
static int get_int_array(const void *blob, int node, const char *prop_name,
		int *array, int count)
{
	const s32 *cell;
	int i, err;

	debug("%s: %s\n", __func__, prop_name);
	cell = get_prop_len(blob, node, prop_name, sizeof(s32) * count, &err);
	if (!err)
		for (i = 0; i < count; i++)
			array[i] = fdt32_to_cpu(cell[i]);
	return err;
}

/**
 * Look up a property in a node and return its contents in a byte
 * array of given length. The property must have at least enough data for
 * the array (count bytes). It may have more, but this will be ignored.
 *
 * @param blob		FDT blob
 * @param node		node to examine
 * @param prop_name	name of property to find
 * @param array		array to fill with data
 * @param count		number of array elements
 * @return 0 if ok, or -FDT_ERR_MISSING if the property is not found,
 *		or -FDT_ERR_BADLAYOUT if not enough data
 */
static int get_byte_array(const void *blob, int node, const char *prop_name,
		u8 *array, int count)
{
	const u8 *cell;
	int err;

	debug("%s: %s\n", __func__, prop_name);
	cell = get_prop_len(blob, node, prop_name, count, &err);
	if (!err)
		memcpy(array, cell, count);
	return err;
}

/**
 * Look up a phandle and follow it to its node. Then return the offset
 * of that node.
 *
 * @param blob		FDT blob
 * @param node		node to examine
 * @param prop_name	name of property to find
 * @return node offset if found, -ve error code on error
 */
static int lookup_phandle(const void *blob, int node, const char *prop_name)
{
	const u32 *phandle;
	int lookup;

	phandle = fdt_getprop(blob, node, prop_name, NULL);
	if (!phandle)
		return -FDT_ERR_NOTFOUND;

	lookup = fdt_node_offset_by_phandle(blob, fdt32_to_cpu(*phandle));
	return lookup;
}

/**
 * Look up a phandle and follow it to its node. Then return the register
 * address of that node as a pointer. This can be used to access the
 * peripheral directly.
 *
 * @param blob		FDT blob
 * @param node		node to examine
 * @param prop_name	name of property to find
 * @return pointer to node's register address
 */
static void *lookup_phandle_reg(const void *blob, int node,
		const char *prop_name)
{
	int lookup;

	lookup = lookup_phandle(blob, node, prop_name);
	if (lookup < 0)
		return NULL;
	return (void *)get_addr(blob, lookup, "reg");
}

/**
 * Checks whether a node is enabled.
 * This looks for a 'status' property. If this exists, then returns 1 if
 * the status is 'ok' and 0 otherwise. If there is no status property,
 * it returns the default value.
 *
 * @param blob	FDT blob
 * @param node	node to examine
 * @param default_val	default value to return if no 'status' property exists
 * @return integer value 0/1, if found, or default_val if not
 */
static int get_is_enabled(const void *blob, int node, int default_val)
{
	const char *cell;

	cell = fdt_getprop(blob, node, "status", NULL);
	if (cell)
		return 0 == strcmp(cell, "ok");
	return default_val;
}

void fdt_decode_uart_calc_divisor(struct fdt_uart *uart)
{
	if (uart->multiplier && uart->baudrate)
		uart->divisor = (uart->clock_freq +
				(uart->baudrate * (uart->multiplier / 2))) /
			(uart->multiplier * uart->baudrate);
}

int fdt_decode_uart_console(const void *blob, struct fdt_uart *uart,
		int default_baudrate)
{
	int node;

	node = find_alias_node(blob, "console");
	if (node < 0)
		return node;
	uart->reg = get_addr(blob, node, "reg");
	uart->id = get_int(blob, node, "id", 0);
	uart->reg_shift = get_int(blob, node, "reg_shift", 2);
	uart->baudrate = get_int(blob, node, "baudrate", default_baudrate);
	uart->clock_freq = get_int(blob, node, "clock-frequency", -1);
	uart->multiplier = get_int(blob, node, "multiplier", 16);
	uart->divisor = get_int(blob, node, "divisor", -1);
	uart->enabled = get_is_enabled(blob, node, 1);
	uart->interrupt = get_int(blob, node, "interrupts", -1);
	uart->silent = fdt_decode_get_config_int(blob, "silent_console", 0);
	uart->io_mapped = get_int(blob, node, "io-mapped", 0);
	uart->compat = fdt_decode_lookup(blob, node);

	/* Calculate divisor if required */
	if ((uart->divisor == -1) && (uart->clock_freq != -1))
		fdt_decode_uart_calc_divisor(uart);
	return 0;
}

enum fdt_compat_id fdt_decode_lookup(const void *blob, int node)
{
	enum fdt_compat_id id;

	/* Search our drivers */
	for (id = COMPAT_UNKNOWN; id < COMPAT_COUNT; id++)
		if (0 == fdt_node_check_compatible(blob, node,
				compat_names[id]))
			return id;
	return COMPAT_UNKNOWN;
}

int fdt_decode_next_compatible(const void *blob, int node,
		enum fdt_compat_id id)
{
	return fdt_node_offset_by_compatible(blob, node, compat_names[id]);
}

int fdt_decode_next_alias(const void *blob, const char *name,
		enum fdt_compat_id id, int *upto)
{
#define MAX_STR_LEN 20
	char str[MAX_STR_LEN + 20];
	int node, err;

	sprintf(str, "%.*s%d", MAX_STR_LEN, name, *upto);
	(*upto)++;
	node = find_alias_node(blob, str);
	if (node < 0)
		return node;
	err = fdt_node_check_compatible(blob, node, compat_names[id]);
	if (err < 0)
		return err;
	return err ? -FDT_ERR_MISSING : node;
}

#ifdef CONFIG_SYS_NS16550
int fdt_decode_get_spi_switch(const void *blob, struct fdt_spi_uart *config)
{
	int node, uart_node;
	const u32 *gpio;

	node = fdt_node_offset_by_compatible(blob, 0,
					     "nvidia,spi-uart-switch");
	if (node < 0)
		return node;

	uart_node = lookup_phandle(blob, node, "uart");
	if (uart_node < 0)
		return uart_node;
	config->port = get_int(blob, uart_node, "id", -1);
	if (config->port == -1)
		return -FDT_ERR_NOTFOUND;
	config->gpio = -1;
	config->regs = (NS16550_t)get_addr(blob, uart_node, "reg");
	gpio = fdt_getprop(blob, node, "gpios", NULL);
	if (gpio)
		config->gpio = fdt32_to_cpu(gpio[1]);
	return 0;
}
#endif

int fdt_decode_memory(const void *blob, const char *name,
		      struct fdt_memory *config)
{
	int node, len;
	const addr_t *cell;

	node = fdt_path_offset(blob, name);
	if (node < 0)
		return node;

	cell = fdt_getprop(blob, node, "reg", &len);
	if (cell && len == sizeof(addr_t) * 2) {
		config->start = addr_to_cpu(cell[0]);
		config->end = addr_to_cpu(cell[1]);
	} else
		return -FDT_ERR_BADLAYOUT;

	return 0;
}

int fdt_decode_gpios(const void *blob, int node, const char *prop_name,
		struct fdt_gpio_state *gpio, int max_count)
{
	const u32 *cell;
	int len, i;

	debug("%s: %s\n", __func__, prop_name);
	assert(max_count > 0);
	cell = fdt_getprop(blob, node, prop_name, &len);
	if (!cell) {
		debug("FDT: %s: property '%s' missing\n", __func__, prop_name);
		return -FDT_ERR_MISSING;
	}

	len /= sizeof(u32) * 3;		/* 3 cells per GPIO record */
	if (len > max_count) {
		printf("FDT: %s: too many GPIOs / cells for "
			"property '%s'\n", __func__, prop_name);
		return -FDT_ERR_BADLAYOUT;
	}
	for (i = 0; i < len; i++, cell += 3) {
		gpio[i].gpio = fdt32_to_cpu(cell[1]);
		gpio[i].flags = fdt32_to_cpu(cell[2]);
	}
	return len;
}

#if 0
/**
 * Decode a list of GPIOs from an FDT. This creates a list of GPIOs with the
 * last one being GPIO_NONE.
 *
 * @param blob		FDT blob to use
 * @param node		Node to look at
 * @param prop_name	Node property name
 * @param gpio		Array of gpio elements to fill from FDT
 * @param max_count	Maximum number of elements allowed, including the
 *			terminator
 * @return 0 if ok, -FDT_ERR_BADLAYOUT if max_count would be exceeded, or
 *		-FDT_ERR_MISSING if the property is missing.
 */
static int decode_gpio_list(const void *blob, int node, const char *prop_name,
		 struct fdt_gpio_state *gpio, int max_count)
{
	int err = fdt_decode_gpios(blob, node, prop_name, gpio, max_count - 1);

	/* terminate the list */
	if (err < 0) {
		debug("FDT: %s: could not decode GPIO "
			"property '%s'\n", __func__, prop_name);
		return err;
	}
	gpio[err].gpio = FDT_GPIO_NONE;
	return 0;
}
#endif

int fdt_decode_gpio(const void *blob, int node, const char *prop_name,
		struct fdt_gpio_state *gpio)
{
	int err;

	debug("%s: %s\n", __func__, prop_name);
	gpio->gpio = FDT_GPIO_NONE;
	err = fdt_decode_gpios(blob, node, prop_name, gpio, 1);
	return err == 1 ? 0 : err;
}

void fdt_setup_gpio(struct fdt_gpio_state *gpio)
{
	if (!fdt_gpio_isvalid(gpio))
		return;

	if (gpio->flags & FDT_GPIO_OUTPUT)
		gpio_direction_output(gpio->gpio, gpio->flags & FDT_GPIO_HIGH);
	else
		gpio_direction_input(gpio->gpio);
}

void fdt_setup_gpios(struct fdt_gpio_state *gpio_list)
{
	struct fdt_gpio_state *gpio;
	int i;

	for (i = 0, gpio = gpio_list; fdt_gpio_isvalid(gpio); i++, gpio++) {
		if (i > FDT_GPIO_MAX) {
			/* Something may have gone horribly wrong */
			printf("FDT: %s: too many GPIOs\n", __func__);
			return;
		}
		fdt_setup_gpio(gpio);
	}
}

int fdt_get_gpio_num(struct fdt_gpio_state *gpio)
{
	return fdt_gpio_isvalid(gpio) ? gpio->gpio : -1;
}

int fdt_decode_lcd(const void *blob, struct fdt_lcd *config)
{
	int node, err, bpp, bit;
	int display_node;

	node = fdt_node_offset_by_compatible(blob, 0, "nvidia,tegra2-lcd");
	if (node < 0)
		return node;
	display_node = lookup_phandle(blob, node, "display");
	if (display_node < 0)
		return display_node;
	config->reg = get_addr(blob, display_node, "reg");
	config->width = get_int(blob, node, "width", -1);
	config->height = get_int(blob, node, "height", -1);
	bpp = get_int(blob, node, "bits_per_pixel", -1);
	bit = ffs(bpp) - 1;
	if (bpp == (1 << bit))
		config->log2_bpp = bit;
	else
		config->log2_bpp = bpp;
	config->bpp = bpp;
	config->pwfm = (struct pwfm_ctlr *)lookup_phandle_reg(blob, node,
							      "pwfm");
	config->disp = (struct disp_ctlr *)lookup_phandle_reg(blob, node,
							  "display");
	config->pixel_clock = get_int(blob, node, "pixel_clock", 0);
	config->cache_type = get_int(blob, node, "cache-type",
			FDT_LCD_CACHE_WRITE_BACK_FLUSH);
	err = get_int_array(blob, node, "horiz_timing", config->horiz_timing,
			FDT_LCD_TIMING_COUNT);
	if (!err)
		err = get_int_array(blob, node, "vert_timing",
				config->vert_timing, FDT_LCD_TIMING_COUNT);
	if (err)
		return err;
	if (!config->pixel_clock || config->reg == -1U || bpp == -1 ||
			config->width == -1 || config->height == -1 ||
			!config->pwfm || !config->disp)
		return -FDT_ERR_MISSING;
	config->frame_buffer = get_addr(blob, node, "frame-buffer");

	err |= fdt_decode_gpio(blob, node, "backlight-enable",
			   &config->backlight_en);
	err |= fdt_decode_gpio(blob, node, "lvds-shutdown",
			   &config->lvds_shutdown);
	fdt_decode_gpio(blob, node, "backlight-vdd", &config->backlight_vdd);
	err |= fdt_decode_gpio(blob, node, "panel-vdd", &config->panel_vdd);
	if (err)
		return -FDT_ERR_MISSING;

	return get_int_array(blob, node, "panel-timings",
			config->panel_timings, FDT_LCD_TIMINGS);
}

int fdt_decode_usb(const void *blob, int node, unsigned osc_frequency_mhz,
		struct fdt_usb *config)
{
	int clk_node = 0, rate;

	/* Find the parameters for our oscillator frequency */
	do {
		clk_node = fdt_node_offset_by_compatible(blob, clk_node,
					"nvidia,tegra250-usbparams");
		if (clk_node < 0)
			return -FDT_ERR_MISSING;
		rate = get_int(blob, clk_node, "osc-frequency", 0);
	} while (rate != osc_frequency_mhz);

	config->reg = (struct usb_ctlr *)get_addr(blob, node, "reg");
	config->host_mode = get_int(blob, node, "host-mode", 0);
	config->utmi = lookup_phandle(blob, node, "utmi") >= 0;
	config->enabled = get_is_enabled(blob, node, 1);
	config->periph_id = get_int(blob, node, "periph-id", -1);
	if (config->periph_id == -1)
		return -FDT_ERR_MISSING;

	return get_int_array(blob, clk_node, "params", config->params,
			PARAM_COUNT);
}

int fdt_decode_sdmmc(const void *blob, int node, struct fdt_sdmmc *config)
{
	config->reg = (struct tegra_mmc *)get_addr(blob, node, "reg");
	config->enabled = get_is_enabled(blob, node, 1);
	config->periph_id = get_int(blob, node, "periph-id", -1);
	config->width = get_int(blob, node, "width", -1);
	config->removable = get_int(blob, node, "removable", 1);
	if (config->periph_id == -1 || config->width == -1)
		return -FDT_ERR_MISSING;

	/* These GPIOs are optional */
	fdt_decode_gpio(blob, node, "cd-gpio", &config->cd_gpio);
	fdt_decode_gpio(blob, node, "wp-gpio", &config->wp_gpio);
	fdt_decode_gpio(blob, node, "power-gpio", &config->power_gpio);
	return 0;
}

const char *fdt_decode_get_model(const void *blob)
{
	const char *model;

	model = fdt_getprop(blob, 0, "model", NULL);
	return model ? model : "<not defined>";
}

int fdt_decode_get_machine_arch_id(const void *blob)
{
	return fdt_decode_get_config_int(blob, "machine-arch-id", -1);
}

char *fdt_decode_get_config_string(const void *blob, const char *prop_name)
{
	const char *nodep;
	int nodeoffset;
	int len;

	debug("%s: %s\n", __func__, prop_name);
	nodeoffset = fdt_path_offset(blob, "/config");
	if (nodeoffset < 0)
		return NULL;

	nodep = fdt_getprop(blob, nodeoffset, prop_name, &len);
	if (!nodep)
		return NULL;

	return (char *)nodep;
}

int fdt_decode_get_config_int(const void *blob, const char *prop_name,
		int default_val)
{
	int config_node;

	debug("%s: %s\n", __func__, prop_name);
	config_node = fdt_path_offset(blob, "/config");
	if (config_node < 0)
		return default_val;
	return get_int(blob, config_node, prop_name, default_val);
}

int fdt_decode_get_config_bool(const void *blob, const char *prop_name)
{
	int config_node;
	const void *prop;

	debug("%s: %s\n", __func__, prop_name);
	config_node = fdt_path_offset(blob, "/config");
	if (config_node < 0)
		return 0;
	prop = fdt_get_property(blob, config_node, prop_name, NULL);

	return prop != NULL;
}

int fdt_decode_kbc(const void *blob, int node, struct fdt_kbc *config)
{
	int err;

	memset(config, '\0', sizeof(*config));
	err = get_byte_array(blob, node, "keycode-plain",
			config->plain_keycode, FDT_KBC_KEY_COUNT);
	if (!err)
		err = get_byte_array(blob, node, "keycode-shift",
				config->shift_keycode, FDT_KBC_KEY_COUNT);

	/* Some keyboards don't have a Fn key */
	if (!err)
		get_byte_array(blob, node, "keycode-fn",
				config->fn_keycode, FDT_KBC_KEY_COUNT);
	if (!err)
		err = get_byte_array(blob, node, "keycode-ctrl",
				config->ctrl_keycode, FDT_KBC_KEY_COUNT);
	return err;
}

int fdt_decode_i2c(const void *blob, int node, struct fdt_i2c *config)
{
	config->reg = (struct i2c_ctlr *)get_addr(blob, node, "reg");
	config->pinmux = get_int(blob, node, "pinmux", 0);
	config->speed = get_int(blob, node, "speed", 0);
	config->periph_id = get_int(blob, node, "periph-id", -1);
	config->use_dvc_ctlr = get_bool(blob, node, "use-dvc-ctlr");

	if (config->periph_id == -1)
		return -FDT_ERR_MISSING;

	return 0;
}

int fdt_decode_nand(const void *blob, int node, struct fdt_nand *config)
{
	int err;

	config->page_data_bytes = get_int(blob, node, "page-data-bytes", -1);
	config->tag_ecc_bytes = get_int(blob, node, "tag-ecc-bytes", -1);
	config->tag_bytes = get_int(blob, node, "tag-bytes", -1);
	config->data_ecc_bytes = get_int(blob, node, "data-ecc-bytes", -1);
	config->skipped_spare_bytes = get_int(blob, node,
			"skipped-spare-bytes", -1);
	config->page_spare_bytes = get_int(blob, node, "page-spare-bytes", -1);
	if (config->page_data_bytes == -1 || config->tag_ecc_bytes == -1 ||
		config->tag_bytes == -1 || config->data_ecc_bytes == -1 ||
		config->skipped_spare_bytes == -1 ||
		config->page_spare_bytes == -1)
		return -FDT_ERR_MISSING;
	err = get_int_array(blob, node, "timing", config->timing,
			     FDT_NAND_TIMING_COUNT);
	if (err < 0)
		return err;

	/* Now look up the controller and decode that */
	node = lookup_phandle(blob, node, "controller");
	if (node < 0)
		return node;
	config->reg = (struct nand_ctlr *)get_addr(blob, node, "reg");
	config->enabled = get_is_enabled(blob, node, 1);
	config->width = get_int(blob, node, "width", 8);
	return fdt_decode_gpio(blob, node, "wp-gpio", &config->wp_gpio);
}

void *fdt_decode_alloc_region(const void *blob, int node,
		const char *prop_name, size_t *size)
{
	const addr_t *cell;
	void *ptr;
	int len;

	debug("%s: %s\n", __func__, prop_name);
	cell = fdt_getprop(blob, node, prop_name, &len);
	if (!cell || (len != sizeof(addr_t) * 2))
		return NULL;

	ptr = (void *)addr_to_cpu(*cell);
	*size = size_to_cpu(cell[1]);
	debug("%s: size=%zx\n", __func__, *size);
	if (!ptr)
		ptr = malloc(*size);
	return ptr;
}
