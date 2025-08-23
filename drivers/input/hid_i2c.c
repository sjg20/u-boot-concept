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
	u8			prev_keys[6]; /* Previous keyboard report */
};

/*
 * HID usage table for keyboard - maps HID usage codes to Linux keycodes
 * Based on USB HID specification and existing keyboard drivers
 */

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

	// log_debug("%s: Reading register 0x%04x, length %d from device 0x%02x\n",
		  // dev->name, reg, len, priv->addr);

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

	// log_debug("About to perform I2C transaction: addr=0x%02x, reg=0x%04x, len=%d\n",
		  // priv->addr, reg, len);
	
	/* Ensure data buffer is zeroed before read */
	// log_debug("memset\n");
	memset(data, 0, len);
	
	// log_debug("i2c\n");
	ret = dm_i2c_xfer(dev, msgs, 2);
	if (ret) {
		log_debug("I2C transfer failed: %d\n", ret);
	} else {
		// log_debug("I2C transfer successful\n");
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

	/* First, read just the descriptor length */
	u16 desc_len;
	ret = hid_i2c_read_register(dev, priv->desc_addr, (u8 *)&desc_len, 2);
	if (ret) {
		printf("Failed to read HID descriptor length: %d\n", ret);
		return ret;
	}
	
	desc_len = le16_to_cpu(desc_len);
	printf("HID descriptor reports length: %d bytes\n", desc_len);
	
	if (desc_len < 30 || desc_len > sizeof(priv->desc)) {
		printf("Invalid HID descriptor length: %d, trying alternative address 0x%04x\n", 
		       desc_len, HID_I2C_ALT_DESC_ADDR);
		
		/* Try alternative descriptor address */
		ret = hid_i2c_read_register(dev, HID_I2C_ALT_DESC_ADDR, (u8 *)&desc_len, 2);
		if (ret) {
			printf("Failed to read from alternative address: %d\n", ret);
			return ret;
		}
		
		desc_len = le16_to_cpu(desc_len);
		printf("Alternative address reports length: %d bytes\n", desc_len);
		
		if (desc_len < 30 || desc_len > sizeof(priv->desc)) {
			printf("Both addresses failed, skipping HID descriptor\n");
			printf("Device may not be standard HID over I2C - using direct keyboard polling\n");
			
			/* Skip HID descriptor, set up for direct keyboard access */
			priv->command_reg = 0x0000;
			priv->data_reg = 0x0000;
			priv->input_reg = 0x0000;  /* Try reading from register 0 */
			priv->max_input_len = 8;   /* Standard keyboard report size */
			
			return 0;  /* Success - we'll try direct access */
		}
		
		priv->desc_addr = HID_I2C_ALT_DESC_ADDR;
	}
	
	/* Now read the full descriptor based on reported length */
	for (retry = 0; retry < HID_I2C_MAX_RETRIES; retry++) {
		/* Clear the descriptor buffer before reading */
		memset(&priv->desc, 0, sizeof(priv->desc));
		
		printf("HID I2C: Reading %d bytes from descriptor addr 0x%04x\n", 
		       desc_len, priv->desc_addr);
		
		ret = hid_i2c_read_register(dev, priv->desc_addr, 
					   (u8 *)&priv->desc, desc_len);
		printf("HID I2C: Read result: %d\n", ret);
		
		if (ret == 0) {
			printf("HID I2C: Full descriptor data: ");
			u8 *raw = (u8 *)&priv->desc;
			for (int i = 0; i < desc_len; i++) {
				printf("%02x ", raw[i]);
			}
			printf("\n");
			break;
		}
		
		printf("HID descriptor read attempt %d failed: %d\n", retry + 1, ret);
		if (retry < HID_I2C_MAX_RETRIES - 1)
			mdelay(50);
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

	printf("HID descriptor raw: wMaxInputLength=0x%04x (%d)\n", 
	       priv->desc.wMaxInputLength, le16_to_cpu(priv->desc.wMaxInputLength));

	if (priv->max_input_len > HID_I2C_MAX_INPUT_LENGTH)
		priv->max_input_len = HID_I2C_MAX_INPUT_LENGTH;
	
	/* If max_input_len is 0, set a reasonable default */
	if (priv->max_input_len == 0) {
		printf("HID descriptor has invalid max_input_len=0, using default 64\n");
		priv->max_input_len = 64;
	}

	log_debug("HID descriptor: cmd_reg=0x%04x, data_reg=0x%04x, input_reg=0x%04x\n",
		  priv->command_reg, priv->data_reg, priv->input_reg);
	panic("check it");

	return 0;
}


static int hid_i2c_read_keys(struct input_config *input)
{
	struct udevice *dev = input->dev;
	struct hid_i2c_priv *priv = dev_get_priv(dev);
	int ret, i, j;
	bool found;
	u8 *cur_keys;

	/* Read input data from device */
	if (!priv->max_input_len) {
		return 0;
	}

	memset(priv->input_buf, '\0', HID_I2C_MAX_INPUT_LENGTH);
	ret = hid_i2c_read_register(dev, priv->input_reg, 
				   priv->input_buf, priv->max_input_len);
	if (ret) {
		return ret;
	}

	/* Standard HID keyboard reports have modifiers in byte 0 and keys in 2-7 */
	if (priv->max_input_len < 8) {
		return 0; /* Not a valid keyboard report */
	}

	/* Assume the keycodes start at offset 2 of the report */
	cur_keys = &priv->input_buf[2];

	/* Check for released keys */
	for (i = 0; i < 6; i++) {
		if (priv->prev_keys[i]) {
			found = false;
			for (j = 0; j < 6; j++) {
				if (priv->prev_keys[i] == cur_keys[j]) {
					found = true;
					break;
				}
			}
			if (!found) {
				input_add_keycode(input, priv->prev_keys[i], true);
			}
		}
	}

	/* Check for pressed keys */
	for (i = 0; i < 6; i++) {
		if (cur_keys[i]) {
			found = false;
			for (j = 0; j < 6; j++) {
				if (cur_keys[i] == priv->prev_keys[j]) {
					found = true;
					break;
				}
			}
			if (!found) {
				input_add_keycode(input, cur_keys[i], false);
			}
		}
	}

	/* Save current keys for next time */
	memcpy(priv->prev_keys, cur_keys, 6);

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


static int hid_i2c_update_leds(struct udevice *dev, int leds)
{
	/* TODO: Implement LED update if supported by device */
	return 0;
}

static const struct keyboard_ops hid_i2c_ops = {
	.start		= hid_i2c_start,
	.stop		= hid_i2c_stop,
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
	
	printf("HID I2C: Device %s at I2C address 0x%02x\n", dev->name, (unsigned)priv->addr);
	
	/* Try to identify the device by reading common ID registers */
	u8 id_buf[4];
	int id_found = 0;
	
	/* Try reading from various common ID register addresses */
	u16 id_addrs[] = {0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0010, 0x0020, 0x00FE, 0x00FF};
	
	printf("HID I2C: Probing for device identification:\n");
	for (int j = 0; j < ARRAY_SIZE(id_addrs); j++) {
		memset(id_buf, 0, sizeof(id_buf));
		ret = hid_i2c_read_register(dev, id_addrs[j], id_buf, 4);
		if (ret == 0) {
			/* Check if we got non-zero, non-0xFF data */
			if (id_buf[0] != 0 || id_buf[1] != 0 || id_buf[2] != 0 || id_buf[3] != 0) {
				if (!(id_buf[0] == 0xFF && id_buf[1] == 0xFF && id_buf[2] == 0xFF && id_buf[3] == 0xFF)) {
					printf("  Addr 0x%04x: %02x %02x %02x %02x\n", 
					       id_addrs[j], id_buf[0], id_buf[1], id_buf[2], id_buf[3]);
					id_found = 1;
				}
			}
		}
	}
	
	if (!id_found) {
		printf("  No readable ID registers found\n");
	} else {
		/* Try reading more data from address 0x0001 since it showed 0x1e (30 bytes) */
		u8 extended_buf[32];
		memset(extended_buf, 0, sizeof(extended_buf));
		ret = hid_i2c_read_register(dev, 0x0001, extended_buf, 30);
		if (ret == 0) {
			printf("  Full 30-byte read from addr 0x0001:\n    ");
			for (int k = 0; k < 30; k++) {
				printf("%02x ", extended_buf[k]);
				if ((k + 1) % 16 == 0) printf("\n    ");
			}
			printf("\n");
		}
	}

	/* Get HID descriptor address from device tree */
	priv->desc_addr = dev_read_u32_default(dev, "hid-descr-addr", 
					       HID_I2C_DEFAULT_DESC_ADDR);
	
	printf("HID I2C: Using descriptor address 0x%04x\n", priv->desc_addr);
	
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
		printf("HID descriptor read failed (%d), using defaults\n", ret);
		/* Set some default values for testing */
		priv->command_reg = 0x0022;
		priv->data_reg = 0x0023;
		priv->input_reg = 0x0024;
		priv->max_input_len = 64;
		printf("HID I2C: Using default registers - input_reg=0x%04x, max_input_len=%d\n", 
		       priv->input_reg, priv->max_input_len);
	} else {
		printf("HID descriptor read successfully\n");
		printf("HID I2C: From descriptor - input_reg=0x%04x, max_input_len=%d\n", 
		       priv->input_reg, priv->max_input_len);
	}

	/* Initialize input system */
	struct input_config *input = &kbd_priv->input;
	struct stdio_dev *sdev = &kbd_priv->sdev;
	
	// input_init(input, false);   (done by keyboard_pre_probe())
	input_add_tables(input, false);

	/* Register the device */
	input->dev = dev;
	input->read_keys = hid_i2c_read_keys;
	strcpy(sdev->name, dev->name);
	ret = input_stdio_register(sdev);
	if (ret) {
		log_err("Failed to register keyboard with stdio: %d\n", ret);
		return ret;
	}
	
	printf("HID I2C: probe completed successfully for %s\n", dev->name);
	printf("HID I2C: input_reg=0x%04x, max_input_len=%d\n", priv->input_reg, priv->max_input_len);
	printf("HID I2C: keyboard registered with stdio system\n");
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
