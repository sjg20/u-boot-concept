// SPDX-License-Identifier: GPL-2.0+
/*
 * HID over I2C driver for U-Boot
 *
 * This driver implements HID over I2C protocol as specified by Microsoft
 * for simple input devices like keyboards, touchpads, and touchscreens.
 *
 * Copyright (c) 2025
 */

#define LOG_DEBUG
#define LOG_CATEGORY	UCLASS_KEYBOARD

#include <command.h>
#include <dm.h>
#include <env.h>
#include <errno.h>
#include <i2c.h>
#include <input.h>
#include <keyboard.h>
#include <log.h>
#include <malloc.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <dm/device-internal.h>
#include <dm/util.h>

#define HID_I2C_DEFAULT_DESC_ADDR	0x0001
#define HID_I2C_ALT_DESC_ADDR		0x0020	/* Alternative address */
#define HID_I2C_COMMAND_RESET		0x01
#define HID_I2C_COMMAND_GET_REPORT	0x02
#define HID_I2C_COMMAND_SET_REPORT	0x03
#define HID_I2C_COMMAND_GET_IDLE	0x04
#define HID_I2C_COMMAND_SET_IDLE	0x05
#define HID_I2C_COMMAND_GET_PROTOCOL	0x06
#define HID_I2C_COMMAND_SET_PROTOCOL	0x07
#define HID_I2C_COMMAND_SET_POWER	0x08

/* Retry and timing constants for robustness */
#define HID_I2C_MAX_RETRIES		3
#define HID_I2C_RESET_DELAY_MS		100
#define HID_I2C_POWERON_DELAY_MS	200

#define HID_I2C_PWR_ON			0x00
#define HID_I2C_PWR_SLEEP		0x01

#define HID_I2C_MAX_INPUT_LENGTH	64

/* HID descriptor structure according to HID over I2C spec */
struct hid_descriptor {
	__le16 wHIDDescLength;
	__le16 bcdVersion;
	__le16 wReportDescLength;
	__le16 wReportDescRegister;
	__le16 wInputRegister;
	__le16 wMaxInputLength;
	__le16 wOutputRegister;
	__le16 wMaxOutputLength;
	__le16 wCommandRegister;
	__le16 wDataRegister;
	__le16 wVendorID;
	__le16 wProductID;
	__le16 wVersionID;
	__le32 reserved;
} __packed;

/* HID over I2C device private data */
struct hid_i2c_priv {
	uint			addr;		/* I2C device address */
	struct hid_descriptor	desc;
	u16			desc_addr;
	u16			command_reg;
	u16			data_reg;
	u16			input_reg;
	u16			max_input_len;
	bool			powered;
	u8			input_buf[HID_I2C_MAX_INPUT_LENGTH];
};

/*
 * HID usage table for keyboard - maps HID usage codes to Linux keycodes
 * Based on USB HID specification and existing keyboard drivers
 */
static const u8 hid_kbd_keymap[] = {
	KEY_RESERVED, 0xff, 0xff, 0xff,				/* 0x00 - 0x03 */
	KEY_A, KEY_B, KEY_C, KEY_D,				/* 0x04 - 0x07 */
	KEY_E, KEY_F, KEY_G, KEY_H,				/* 0x08 - 0x0b */
	KEY_I, KEY_J, KEY_K, KEY_L,				/* 0x0c - 0x0f */
	KEY_M, KEY_N, KEY_O, KEY_P,				/* 0x10 - 0x13 */
	KEY_Q, KEY_R, KEY_S, KEY_T,				/* 0x14 - 0x17 */
	KEY_U, KEY_V, KEY_W, KEY_X,				/* 0x18 - 0x1b */
	KEY_Y, KEY_Z, KEY_1, KEY_2,				/* 0x1c - 0x1f */
	KEY_3, KEY_4, KEY_5, KEY_6,				/* 0x20 - 0x23 */
	KEY_7, KEY_8, KEY_9, KEY_0,				/* 0x24 - 0x27 */
	KEY_ENTER, KEY_ESC, KEY_BACKSPACE, KEY_TAB,		/* 0x28 - 0x2b */
	KEY_SPACE, KEY_MINUS, KEY_EQUAL, KEY_LEFTBRACE,		/* 0x2c - 0x2f */
	KEY_RIGHTBRACE, KEY_BACKSLASH, 0xff, KEY_SEMICOLON,	/* 0x30 - 0x33 */
	KEY_APOSTROPHE, KEY_GRAVE, KEY_COMMA, KEY_DOT,		/* 0x34 - 0x37 */
	KEY_SLASH, KEY_CAPSLOCK, KEY_F1, KEY_F2,		/* 0x38 - 0x3b */
	KEY_F3, KEY_F4, KEY_F5, KEY_F6,				/* 0x3c - 0x3f */
	KEY_F7, KEY_F8, KEY_F9, KEY_F10,			/* 0x40 - 0x43 */
	KEY_F11, KEY_F12, KEY_SYSRQ, KEY_SCROLLLOCK,		/* 0x44 - 0x47 */
	KEY_PAUSE, KEY_INSERT, KEY_HOME, KEY_PAGEUP,		/* 0x48 - 0x4b */
	KEY_DELETE, KEY_END, KEY_PAGEDOWN, KEY_RIGHT,		/* 0x4c - 0x4f */
	KEY_LEFT, KEY_DOWN, KEY_UP, KEY_NUMLOCK,		/* 0x50 - 0x53 */
};

/* Modifier key mapping for HID keyboard boot protocol */
#define HID_MOD_LEFTCTRL	BIT(0)
#define HID_MOD_LEFTSHIFT	BIT(1)
#define HID_MOD_LEFTALT		BIT(2)
#define HID_MOD_LEFTGUI		BIT(3)
#define HID_MOD_RIGHTCTRL	BIT(4)
#define HID_MOD_RIGHTSHIFT	BIT(5)
#define HID_MOD_RIGHTALT	BIT(6)
#define HID_MOD_RIGHTGUI	BIT(7)

static int hid_i2c_read_register(struct udevice *dev, u16 reg, u8 *data, int len)
{
	struct hid_i2c_priv *priv = dev_get_priv(dev);
	struct i2c_msg msgs[2];
	u8 reg_buf[2];
	int ret;

	/* Safety checks to prevent crashes */
	if (!dev || !priv || !data || len <= 0 || len > 256) {
		log_err("Invalid parameters for I2C read: dev=%p, priv=%p, data=%p, len=%d\n",
			dev, priv, data, len);
		return -EINVAL;
	}
	
	if (!dev->parent) {
		log_err("Device has no parent I2C bus\n");
		return -ENODEV;
	}

	log_debug("%s: Reading register 0x%04x, length %d from device 0x%02x\n",
		  dev->name, reg, len, priv->addr);

	/* Register address is little-endian */
	reg_buf[0] = reg & 0xff;
	reg_buf[1] = (reg >> 8) & 0xff;

	msgs[0].addr = priv->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = reg_buf;

	msgs[1].addr = priv->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = data;

	log_debug("About to perform I2C transaction: addr=0x%02x, reg=0x%04x, len=%d\n",
		  priv->addr, reg, len);
	
	/* Ensure data buffer is zeroed before read */
	log_debug("memset\n");
	memset(data, 0, len);
	
	log_debug("i2c\n");
	ret = dm_i2c_xfer(dev, msgs, 2);
	if (ret) {
		log_debug("I2C transfer failed: %d\n", ret);
	} else {
		log_debug("I2C transfer successful\n");
	}
	return ret;
}

static int hid_i2c_write_register(struct udevice *dev, u16 reg, const u8 *data, int len)
{
	struct hid_i2c_priv *priv = dev_get_priv(dev);
	struct i2c_msg msg;
	u8 *buf;
	int ret;

	buf = malloc(len + 2);
	if (!buf)
		return -ENOMEM;

	/* Register address is little-endian */
	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;
	memcpy(&buf[2], data, len);

	msg.addr = priv->addr;
	msg.flags = 0;
	msg.len = len + 2;
	msg.buf = buf;

	ret = dm_i2c_xfer(dev, &msg, 1);
	free(buf);
	return ret;
}

static int hid_i2c_send_command(struct udevice *dev, u8 command, u8 *args, int arg_len)
{
	struct hid_i2c_priv *priv = dev_get_priv(dev);
	u8 *cmd_buf;
	int ret;

	cmd_buf = malloc(arg_len + 4);
	if (!cmd_buf)
		return -ENOMEM;

	cmd_buf[0] = priv->command_reg & 0xff;
	cmd_buf[1] = (priv->command_reg >> 8) & 0xff;
	cmd_buf[2] = command;
	cmd_buf[3] = 0; /* Report ID */
	
	if (args && arg_len > 0)
		memcpy(&cmd_buf[4], args, arg_len);

	ret = hid_i2c_write_register(dev, priv->command_reg, &cmd_buf[2], arg_len + 2);
	free(cmd_buf);
	return ret;
}

static int hid_i2c_reset(struct udevice *dev)
{
	int ret;

	ret = hid_i2c_send_command(dev, HID_I2C_COMMAND_RESET, NULL, 0);
	if (ret)
		return ret;

	/* Wait for device to reset - longer delay for X1E platforms */
	mdelay(HID_I2C_RESET_DELAY_MS);
	return 0;
}

static int hid_i2c_set_power(struct udevice *dev, bool on)
{
	struct hid_i2c_priv *priv = dev_get_priv(dev);
	u8 power_arg = on ? HID_I2C_PWR_ON : HID_I2C_PWR_SLEEP;
	int ret;

	ret = hid_i2c_send_command(dev, HID_I2C_COMMAND_SET_POWER, &power_arg, 1);
	if (ret)
		return ret;

	priv->powered = on;
	if (on) {
		/* Longer power-on delay for X1E platforms */
		int delay = dev_read_u32_default(dev, "post-power-on-delay-ms", 
						 HID_I2C_POWERON_DELAY_MS);
		mdelay(delay);
	}

	return 0;
}

static int hid_i2c_read_hid_descriptor(struct udevice *dev)
{
	struct hid_i2c_priv *priv = dev_get_priv(dev);
	int ret, retry;
	u8 test_byte;

	if (!priv) {
		log_err("Invalid private data\n");
		return -EINVAL;
	}

	log_debug("Testing basic I2C connectivity first\n");
	
	/* Try a simple 1-byte read first to test I2C connectivity */
	ret = hid_i2c_read_register(dev, 0x0000, &test_byte, 1);
	if (ret) {
		log_err("Basic I2C connectivity test failed: %d\n", ret);
		return ret;
	}
	
	log_debug("Basic I2C test passed, now reading HID descriptor from address 0x%04x\n", priv->desc_addr);

	/* Try reading HID descriptor with retries */
	for (retry = 0; retry < HID_I2C_MAX_RETRIES; retry++) {
		/* Clear the descriptor buffer before reading */
		memset(&priv->desc, 0, sizeof(priv->desc));
		
		ret = hid_i2c_read_register(dev, priv->desc_addr, 
					   (u8 *)&priv->desc, sizeof(priv->desc));
		if (ret == 0) {
			log_debug("HID descriptor read successful on attempt %d\n", retry + 1);
			break;
		}
		
		log_debug("HID descriptor read attempt %d failed: %d\n", retry + 1, ret);
		if (retry < HID_I2C_MAX_RETRIES - 1)
			mdelay(50);  /* Longer delay between retries */
	}
	
	if (ret) {
		log_debug("Failed to read HID descriptor after %d retries: %d\n", 
			HID_I2C_MAX_RETRIES, ret);
		return ret;
	}

	/* Convert from little-endian */
	priv->command_reg = le16_to_cpu(priv->desc.wCommandRegister);
	priv->data_reg = le16_to_cpu(priv->desc.wDataRegister);
	priv->input_reg = le16_to_cpu(priv->desc.wInputRegister);
	priv->max_input_len = le16_to_cpu(priv->desc.wMaxInputLength);

	if (priv->max_input_len > HID_I2C_MAX_INPUT_LENGTH)
		priv->max_input_len = HID_I2C_MAX_INPUT_LENGTH;

	log_debug("HID descriptor: cmd_reg=0x%04x, data_reg=0x%04x, input_reg=0x%04x\n",
		  priv->command_reg, priv->data_reg, priv->input_reg);

	return 0;
}

static int hid_i2c_process_keyboard_report(struct udevice *dev, u8 *data, int len)
{
	struct keyboard_priv *kbd_priv = dev_get_uclass_priv(dev);
	struct input_config *input = &kbd_priv->input;
	u8 modifiers, *keycodes;
	int i, keycode, num_keys = 0;
	int keys[8]; /* Maximum simultaneous keys */

	if (len < 8) {
		log_debug("Keyboard report too short: %d bytes\n", len);
		return -EINVAL;
	}

	/* Skip report length field (first 2 bytes) */
	modifiers = data[2];
	keycodes = &data[4]; /* Skip reserved byte at data[3] */

	/* Process modifier keys */
	if (modifiers & HID_MOD_LEFTCTRL) keys[num_keys++] = KEY_LEFTCTRL;
	if (modifiers & HID_MOD_LEFTSHIFT) keys[num_keys++] = KEY_LEFTSHIFT;
	if (modifiers & HID_MOD_LEFTALT) keys[num_keys++] = KEY_LEFTALT;
	if (modifiers & HID_MOD_LEFTGUI) keys[num_keys++] = KEY_LEFTMETA;
	if (modifiers & HID_MOD_RIGHTCTRL) keys[num_keys++] = KEY_RIGHTCTRL;
	if (modifiers & HID_MOD_RIGHTSHIFT) keys[num_keys++] = KEY_RIGHTSHIFT;
	if (modifiers & HID_MOD_RIGHTALT) keys[num_keys++] = KEY_RIGHTALT;
	if (modifiers & HID_MOD_RIGHTGUI) keys[num_keys++] = KEY_RIGHTMETA;

	/* Process regular keys */
	for (i = 0; i < 6 && num_keys < 8; i++) {
		if (keycodes[i] == 0)
			continue;
		
		if (keycodes[i] < ARRAY_SIZE(hid_kbd_keymap)) {
			keycode = hid_kbd_keymap[keycodes[i]];
			if (keycode != 0xff && keycode != KEY_RESERVED)
				keys[num_keys++] = keycode;
		}
	}

	/* Send keys to input system */
	return input_send_keycodes(input, keys, num_keys);
}

static int hid_i2c_read_keys(struct input_config *input)
{
	struct udevice *dev = input->dev;
	struct hid_i2c_priv *priv = dev_get_priv(dev);
	int ret, len;

	/* Read input data from HID device */
	if (!priv->input_reg || !priv->max_input_len) {
		log_debug("HID I2C: No input register configured\n");
		return 0;
	}

	ret = hid_i2c_read_register(dev, priv->input_reg, 
				   priv->input_buf, priv->max_input_len);
	if (ret) {
		log_debug("HID I2C: Failed to read input data: %d\n", ret);
		return ret;
	}

	/* Get report length from first 2 bytes */
	len = priv->input_buf[0] | (priv->input_buf[1] << 8);
	log_debug("HID I2C: Read %d bytes, report length: %d\n", priv->max_input_len, len);
	
	if (len > 2 && len <= priv->max_input_len) {
		log_debug("HID I2C: Processing keyboard report\n");
		hid_i2c_process_keyboard_report(dev, priv->input_buf, len);
	} else if (len > 0) {
		log_debug("HID I2C: Invalid report length: %d\n", len);
	}

	return 0;
}

static int hid_i2c_start(struct udevice *dev)
{
	int ret;

	printf("HID I2C: Starting HID I2C device %s\n", dev->name);

	/* Power on device */
	ret = hid_i2c_set_power(dev, true);
	if (ret) {
		log_err("Failed to power on device: %d\n", ret);
		return ret;
	}

	/* Reset device */
	ret = hid_i2c_reset(dev);
	if (ret) {
		log_err("Failed to reset device: %d\n", ret);
		return ret;
	}

	return 0;
}

static int hid_i2c_stop(struct udevice *dev)
{
	log_debug("Stopping HID I2C device\n");
	return hid_i2c_set_power(dev, false);
}

static int hid_i2c_tstc(struct udevice *dev)
{
	struct keyboard_priv *kbd_priv = dev_get_uclass_priv(dev);
	int ret;
	
	ret = input_tstc(&kbd_priv->input);
	if (ret)
		printf("HID I2C: tstc called for %s, returning %d\n", dev->name, ret);
	
	return ret;
}

static int hid_i2c_getc(struct udevice *dev)
{
	struct hid_i2c_priv *priv = dev_get_priv(dev);
	struct keyboard_priv *kbd_priv = dev_get_uclass_priv(dev);
	int ret, len;

	printf("HID I2C: getc called for %s\n", dev->name);

	/* Try to read input first if no keys are pending */
	if (!input_tstc(&kbd_priv->input)) {
		log_debug("HID I2C: No keys pending, reading input register 0x%04x\n", priv->input_reg);
		ret = hid_i2c_read_register(dev, priv->input_reg, 
					   priv->input_buf, priv->max_input_len);
		if (ret == 0) {
			/* Get report length from first 2 bytes */
			len = priv->input_buf[0] | (priv->input_buf[1] << 8);
			log_debug("HID I2C: Read %d bytes, report length: %d\n", priv->max_input_len, len);
			if (len > 2 && len <= priv->max_input_len) {
				log_debug("HID I2C: Processing keyboard report\n");
				hid_i2c_process_keyboard_report(dev, priv->input_buf, len);
			} else {
				log_debug("HID I2C: Invalid report length: %d\n", len);
			}
		} else {
			log_debug("HID I2C: Failed to read input register: %d\n", ret);
		}
	} else {
		log_debug("HID I2C: Keys already pending\n");
	}

	/* Return next available key */
	ret = input_getc(&kbd_priv->input);
	if (ret > 0) {
		log_debug("HID I2C: Returning key: 0x%02x ('%c')\n", ret, ret >= 32 ? ret : '?');
	}
	return ret;
}

static int hid_i2c_update_leds(struct udevice *dev, int leds)
{
	/* TODO: Implement LED update if supported by device */
	return 0;
}

static const struct keyboard_ops hid_i2c_ops = {
	.start		= hid_i2c_start,
	.stop		= hid_i2c_stop,
	.tstc		= hid_i2c_tstc,
	.getc		= hid_i2c_getc,
	.update_leds	= hid_i2c_update_leds,
};

static int hid_i2c_probe(struct udevice *dev)
{
	struct hid_i2c_priv *priv = dev_get_priv(dev);
	struct keyboard_priv *kbd_priv = dev_get_uclass_priv(dev);
	int ret;

	printf("HID I2C: probe starting for device %s\n", dev->name);
	log_debug("HID start\n");

	/* Get I2C address */
	priv->addr = dev_read_addr(dev);
	if (priv->addr == FDT_ADDR_T_NONE) {
		log_err("Failed to get I2C address\n");
		return -EINVAL;
	}

	/* Get HID descriptor address from device tree */
	priv->desc_addr = dev_read_u32_default(dev, "hid-descr-addr", 
					       HID_I2C_DEFAULT_DESC_ADDR);
	
	/* Try alternative descriptor address if the first one fails */
	if (priv->desc_addr == HID_I2C_DEFAULT_DESC_ADDR) {
		/* Some devices (especially keyboards) use alternative address */
		if (device_is_compatible(dev, "hid-over-i2c") && 
		    !device_is_compatible(dev, "elan,ekth3000")) {
			priv->desc_addr = HID_I2C_ALT_DESC_ADDR;
		}
	}

	log_debug("HID I2C device at address 0x%02x, descriptor at 0x%04x\n",
		  priv->addr, priv->desc_addr);

	/* Try to read HID descriptor with error checking */
	log_debug("Attempting to read HID descriptor...\n");
	ret = hid_i2c_read_hid_descriptor(dev);
	if (ret) {
		log_debug("HID descriptor read failed (%d), using defaults\n", ret);
		/* Set some default values for testing */
		priv->command_reg = 0x0022;
		priv->data_reg = 0x0023;
		priv->input_reg = 0x0024;
		priv->max_input_len = 64;
	} else {
		log_debug("HID descriptor read successfully\n");
	}

	/* Initialize input system */
	struct input_config *input = &kbd_priv->input;
	
	input->dev = dev;
	input->read_keys = hid_i2c_read_keys;
	ret = input_init(input, 0);
	if (ret) {
		log_err("Failed to initialize input: %d\n", ret);
		return ret;
	}

	printf("HID I2C: Input system initialized with read_keys function\n");
	log_debug("Input system initialized successfully\n");

	ret = input_add_tables(input, false);
	if (ret) {
		log_err("Failed to add input tables: %d\n", ret);
		return ret;
	}
	
	printf("HID I2C: probe completed successfully for %s\n", dev->name);
	printf("HID I2C: input_reg=0x%04x, max_input_len=%d\n", priv->input_reg, priv->max_input_len);
	
	/* Register keyboard with stdio system */
	ret = input_stdio_register(&kbd_priv->sdev);
	if (ret) {
		log_err("Failed to register keyboard with stdio: %d\n", ret);
		return ret;
	}
	
	printf("HID I2C: keyboard registered with stdio system\n");
	log_debug("HID I2C probe completed successfully\n");
	return 0;
}

static const struct udevice_id hid_i2c_ids[] = {
	{ .compatible = "hid-over-i2c" },
	/* Vendor-specific devices commonly found on X1E platforms */
	{ .compatible = "elan,ekth3000" },		/* ELAN touchpads */
	{ .compatible = "elan,ekth3500" },		/* ELAN touchpads (newer) */
	{ .compatible = "synaptics,tm3038-005" },	/* Synaptics touchpads */
	{ .compatible = "synaptics,tm3253-005" },	/* Synaptics touchpads */
	{ .compatible = "wacom,w9013" },		/* Wacom digitizers */
	{ }
};

U_BOOT_DRIVER(hid_i2c) = {
	.name		= "hid_i2c",
	.id		= UCLASS_KEYBOARD,
	.of_match	= hid_i2c_ids,
	.probe		= hid_i2c_probe,
	.priv_auto	= sizeof(struct hid_i2c_priv),
	.ops		= &hid_i2c_ops,
};

/**
 * hid_i2c_init() - Initialize HID over I2C devices late in boot
 *
 * This function manually probes for HID over I2C devices after the display
 * is up and running, allowing debug output to be visible. It should be called
 * from main_loop() just before cli_loop().
 *
 * Return: 0 if successful, negative error code otherwise
 */
int hid_i2c_init(void)
{
	struct udevice *bus, *dev;
	struct uclass *uc;
	int ret, found = 0;

	log_info("HID I2C: Initializing HID over I2C devices...\n");
	// dm_dump_tree(NULL, false, true);

	printf("I2C devices:\n");
	uclass_id_foreach_dev(UCLASS_I2C, dev, uc) {
		printf("I2C device: %s (parent: %s)\n", dev->name, dev->parent ? dev->parent->name : "none");
	}
	printf("--\n");

	printf("NOP devices:\n");
	uclass_id_foreach_dev(UCLASS_NOP, dev, uc) {
		printf("NOP device: %s\n", dev->name);
	}
	printf("--\n");
	printf("stdin: %s\n", env_get("stdin"));
	run_command("coninfo", 0);
	env_set("stdin", "keyboard@3a");
	printf("stdin2: %s\n", env_get("stdin"));

	/* Find all I2C buses - use _check to handle probe failures gracefully */
	for (ret = uclass_first_device_check(UCLASS_I2C, &bus); bus;
	     ret = uclass_next_device_check(&bus)) {
		if (ret) {
			log_info("HID I2C: Skipping I2C bus %s due to probe failure (err=%d)\n",
				  bus->name, ret);
			continue;
		}

		log_info("HID I2C: Successfully probed I2C bus %s (uclass=%d)\n", bus->name, bus->uclass->uc_drv->id);
		
		/* Skip buses that failed to probe */
		if (!device_active(bus)) {
			log_debug("HID I2C: I2C bus %s failed to initialize, skipping\n", bus->name);
			continue;
		}
#if 1
		log_debug("HID I2C: Scanning active I2C bus %s\n", bus->name);

		/* Look for HID over I2C devices on this bus */
		device_foreach_child(dev, bus) {
			log_debug("- dev '%s'\n", dev->name);
			if (device_is_compatible(dev, "hid-over-i2c")) {
				log_info("HID I2C: Found HID device: %s\n", dev->name);
				found++;
#if 0
				/* Probe the device */
				ret = device_probe(dev);
				if (ret) {
					log_err("HID I2C: Failed to probe %s: %d\n", 
						dev->name, ret);
				} else {
					log_info("HID I2C: Successfully probed %s\n", 
						 dev->name);
					found++;
				}
#endif
			}
		}
#endif
	}

	if (found > 0) {
		struct udevice *kdev;

		log_info("HID I2C: Initialized %d HID device(s)\n", found);
		
		/* Try to register HID keyboards with stdio */
		printf("HID I2C: Attempting to add keyboards to stdio\n");
		run_command("coninfo", 0);
		log_debug("searching for keyboards:\n");
		uclass_foreach_dev_probe(UCLASS_KEYBOARD, kdev)
			printf("keyboard: %s\n", kdev->name);
		run_command("coninfo", 0);
		env_set("stdin", "keyboard@3a");
		printf("stdin3: %s\n", env_get("stdin"));

	} else {
		log_info("HID I2C: No HID over I2C devices found\n");
	}

	return found > 0 ? 0 : -ENODEV;
}
