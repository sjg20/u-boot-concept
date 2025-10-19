// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025 Canonical Ltd
 *
 * TKey emulator for testing TKey functionality in sandbox
 */

#define LOG_CATEGORY	UCLASS_TKEY

#include <dm.h>
#include <errno.h>
#include <log.h>
#include <malloc.h>
#include <tkey.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <asm/unaligned.h>

/* TKey protocol frame structure */
#define FRAME_SIZE		128
#define FRAME_HEADER_SIZE	1
#define FRAME_DATA_SIZE		(FRAME_SIZE - FRAME_HEADER_SIZE)

/* Frame header bit masks and values */
#define FRAME_ENDPOINT_MASK	0x18
#define FRAME_ENDPOINT_SHIFT	3
#define ENDPOINT_FIRMWARE	2
#define ENDPOINT_APP		3

/* Firmware Commands */
#define FW_CMD_GET_NAME_VERSION	0x01
#define FW_CMD_GET_UDI		0x08
#define FW_CMD_LOAD_APP		0x03
#define FW_CMD_LOAD_APP_DATA	0x05

/* App Commands */
#define APP_CMD_GET_PUBKEY	0x01

/* USB Response format markers */
#define USB_FRAME_MARKER	0x52
#define USB_RSP_NAME_VERSION	0x02
#define USB_RSP_GET_UDI		0x09

/* Status codes */
#define STATUS_OK		0x00
#define STATUS_ERROR		0x01

/*
 * struct tkey_emul_priv - TKey emulator state
 *
 * @app_loaded: Whether an app is loaded (app mode vs firmware mode)
 * @udi: Unique Device Identifier (8 bytes)
 * @app_size: Size of loaded app
 * @pubkey: Simulated public key (32 bytes)
 * @resp: Buffer for storing response to be read
 * @resp_len: Length of data in response buffer
 * @total_loaded: Track total app data loaded
 */
struct tkey_emul_priv {
	bool app_loaded;
	u8 udi[8];
	u32 app_size;
	u8 pubkey[32];
	u8 resp[FRAME_SIZE];
	int resp_len;
	u32 total_loaded;
};

static int tkey_emul_read(struct udevice *dev, void *buf, int len,
			  int timeout_ms)
{
	/*
	 * Read operations are immediate with no actual I/O. The data is
	 * prepared by write operations in the emulated response buffer
	 */
	log_debug("read: %d bytes requested\n", len);

	return -ENOSYS;
}

static int handle_fw_get_name_version(struct tkey_emul_priv *priv)
{
	/* USB format: 0x52 0x02 [tk1 ] [name1] [version] */
	priv->resp[0] = USB_FRAME_MARKER;
	priv->resp[1] = USB_RSP_NAME_VERSION;
	memcpy(priv->resp + 2, "tk1 ", 4);

	/* name1 changes based on firmware vs app mode */
	if (priv->app_loaded)
		memcpy(priv->resp + 6, "sign", 4);
	else
		memcpy(priv->resp + 6, "mkdf", 4);
	put_unaligned_le32(4, priv->resp + 10);
	priv->resp_len = 14;
	log_debug("GET_NAME_VERSION (mode=%s)\n",
		  priv->app_loaded ? "app" : "firmware");

	return 0;
}

static int handle_fw_get_udi(struct tkey_emul_priv *priv)
{
	/* UDI is only available in firmware mode */
	if (priv->app_loaded) {
		priv->resp_len = 0;
		log_debug("GET_UDI rejected (app mode)\n");
	} else {
		priv->resp[0] = USB_FRAME_MARKER;
		priv->resp[1] = USB_RSP_GET_UDI;
		priv->resp[2] = STATUS_OK;
		memcpy(priv->resp + 3, priv->udi, 8);
		priv->resp_len = 11;
		log_debug("GET_UDI OK\n");
	}

	return 0;
}

static int handle_fw_load_app(struct tkey_emul_priv *priv, const u8 *data)
{
	/* App size is in bytes 2-5 (big endian) */
	priv->app_size = get_unaligned_be32(data + 2);

	/* Simple ACK - just return status */
	priv->resp[0] = STATUS_OK;
	priv->resp_len = 1;
	log_debug("LOAD_APP (size=%u)\n", priv->app_size);

	return 0;
}

static int handle_fw_load_app_data(struct tkey_emul_priv *priv, const u8 *data)
{
	int chunk_size = get_unaligned_be32(data + 2);

	priv->total_loaded += chunk_size;

	/* Simple ACK */
	priv->resp[0] = STATUS_OK;
	priv->resp_len = 1;

	if (priv->total_loaded >= priv->app_size) {
		/* App fully loaded - enter app mode */
		priv->app_loaded = true;
		priv->total_loaded = 0;
		log_debug("App loaded, entering app mode\n");
	} else {
		log_debug("LOAD_APP_DATA (%u/%u)\n",
			  priv->total_loaded, priv->app_size);
	}

	return 0;
}

static int handle_firmware_cmd(struct udevice *dev, u8 cmd, const u8 *data)
{
	struct tkey_emul_priv *priv = dev_get_priv(dev);

	switch (cmd) {
	case FW_CMD_GET_NAME_VERSION:
		return handle_fw_get_name_version(priv);
	case FW_CMD_GET_UDI:
		return handle_fw_get_udi(priv);
	case FW_CMD_LOAD_APP:
		return handle_fw_load_app(priv, data);
	case FW_CMD_LOAD_APP_DATA:
		return handle_fw_load_app_data(priv, data);
	default:
		log_err("Unknown firmware command %02x\n", cmd);
		return -EINVAL;
	}
}

static int handle_app_get_pubkey(struct tkey_emul_priv *priv)
{
	memcpy(priv->resp, priv->pubkey, 32);
	priv->resp_len = 32;
	log_debug("GET_PUBKEY\n");

	return 0;
}

static int handle_app_cmd(struct udevice *dev, u8 cmd)
{
	struct tkey_emul_priv *priv = dev_get_priv(dev);

	if (!priv->app_loaded) {
		log_err("App command sent but not in app mode\n");
		return -EINVAL;
	}

	switch (cmd) {
	case APP_CMD_GET_PUBKEY:
		return handle_app_get_pubkey(priv);
	default:
		log_err("Unknown app command %02x\n", cmd);
		return -EINVAL;
	}
}

static int tkey_emul_write(struct udevice *dev, const void *buf, int len)
{
	const u8 *data = buf;
	u8 header, endpoint, cmd;
	int ret;

	if (len < 2)
		return -EINVAL;

	header = data[0];
	endpoint = (header & FRAME_ENDPOINT_MASK) >> FRAME_ENDPOINT_SHIFT;
	cmd = data[1];

	log_debug("header %02x endpoint %u cmd %02x\n", header, endpoint, cmd);

	/* Route to appropriate endpoint handler */
	if (endpoint == ENDPOINT_FIRMWARE) {
		ret = handle_firmware_cmd(dev, cmd, data);
	} else if (endpoint == ENDPOINT_APP) {
		ret = handle_app_cmd(dev, cmd);
	} else {
		log_err("Unknown endpoint %u\n", endpoint);
		return -EINVAL;
	}

	return ret ? ret : len;
}

static int tkey_emul_read_all(struct udevice *dev, void *buf, int maxlen,
			      int timeout_ms)
{
	struct tkey_emul_priv *priv = dev_get_priv(dev);
	int len = min(priv->resp_len, maxlen);

	log_debug("read_all: %d bytes max, returning %d bytes\n", maxlen, len);

	/* Copy the raw USB response data including the 0x52 marker */
	if (len > 0)
		memcpy(buf, priv->resp, len);

	return len;
}

static int tkey_emul_probe(struct udevice *dev)
{
	struct tkey_emul_priv *priv = dev_get_priv(dev);
	int i;

	/* Generate a deterministic UDI based on device name */
	for (i = 0; i < 8; i++)
		priv->udi[i] = 0xa0 + i;

	/* Generate a deterministic public key */
	for (i = 0; i < 32; i++)
		priv->pubkey[i] = 0x50 + (i & 0xf);

	log_debug("init with UDI: ");
	for (i = 0; i < 8; i++)
		log_debug("%02x", priv->udi[i]);
	log_debug("\n");

	return 0;
}

/* TKey uclass operations */
static const struct tkey_ops tkey_emul_ops = {
	.read = tkey_emul_read,
	.write = tkey_emul_write,
	.read_all = tkey_emul_read_all,
};

static const struct udevice_id tkey_emul_ids[] = {
	{ .compatible = "tkey,emul" },
	{ }
};

U_BOOT_DRIVER(tkey_emul) = {
	.name		= "tkey_emul",
	.id		= UCLASS_TKEY,
	.of_match	= tkey_emul_ids,
	.probe		= tkey_emul_probe,
	.ops		= &tkey_emul_ops,
	.priv_auto	= sizeof(struct tkey_emul_priv),
};
