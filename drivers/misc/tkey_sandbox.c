// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025 Canonical Ltd
 *
 * Sandbox TKey driver for testing TKey functionality in sandbox
 * Communicates with TKey devices via /dev/ttyACM0
 */

#define LOG_CATEGORY	UCLASS_TKEY

#include <dm.h>
#include <errno.h>
#include <log.h>
#include <malloc.h>
#include <os.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <asm/unaligned.h>
#include <tkey.h>

/*
 * struct tkey_sandbox_priv - private information about sandbox
 *
 * @path: Path to the tkey device, e.g. "/dev/ttyACM0"
 * @fd: File descriptor
 */
struct tkey_sandbox_priv {
	char *path;
	int fd;
};

static int tkey_sandbox_read(struct udevice *dev, void *buffer, int len,
			     int timeout_ms)
{
	struct tkey_sandbox_priv *priv = dev_get_priv(dev);
	u8 *buf = buffer;
	int total, ret;

	if (priv->fd < 0)
		return -ENODEV;

	log_debug("Reading %d bytes...\n", len);

	/* Read data in chunks until we get the full amount */
	for (total = 0; total < len; total += ret) {
		ret = os_read(priv->fd, buf + total, len - total);
		log_debug("Read attempt returned: %d (total: %d/%d)\n", ret,
			  total, len);

		if (ret < 0) {
			log_debug("Read failed with error %d\n", ret);
			return -EIO;
		}

		if (!ret) {
			if (!total) {
				log_debug("Read timeout - no data received\n");
				return -EIO;
			}
			/* Partial read - break and return what we got */
			log_debug("Partial read, got %x/%x bytes\n", total,
				  len);
			break;
		}
	}

	log_debug("Read %d bytes:", total);
	for (int i = 0; i < total; i++)
		log_debug(" %02x", buf[i]);
	log_debug("\n");

	return total;
}

static int tkey_sandbox_write(struct udevice *dev, const void *buffer, int len)
{
	struct tkey_sandbox_priv *priv = dev_get_priv(dev);
	int ret;

	if (priv->fd < 0)
		return -ENODEV;

	log_debug("Writing %d bytes:", len);
	for (int i = 0; i < len; i++)
		log_debug(" %02x", ((u8*)buffer)[i]);
	log_debug("\n");

	ret = os_write(priv->fd, buffer, len);
	if (ret < 0) {
		log_debug("Write failed with error %d\n", ret);
		return -EIO;
	}
	log_debug("Wrote %d bytes\n", ret);

	return ret;
}

static int tkey_sandbox_probe(struct udevice *dev)
{
	struct tkey_sandbox_priv *priv = dev_get_priv(dev);
	const char *device_path;

	/* Get device path from device tree or use default */
	device_path = dev_read_string(dev, "sandbox,device-path");
	if (!device_path)
		device_path = "/dev/ttyACM0";

	priv->path = strdup(device_path);
	if (!priv->path)
		return -ENOMEM;

	/* Open the serial device */
	priv->fd = os_open(priv->path, OS_O_RDWR);
	if (priv->fd < 0) {
		log_err("Failed to open %s (error %d)\n", priv->path, priv->fd);
		free(priv->path);
		return -ENODEV;
	}

	/* Configure serial port for raw mode */
	if (os_tty_set_params(priv->fd) < 0) {
		log_err("Failed to configure serial port %s\n", priv->path);
		os_close(priv->fd);
		free(priv->path);
		return -ENODEV;
	}
	log_debug("Connected to %s with serial parameters configured\n",
		  priv->path);

	return 0;
}

static int tkey_sandbox_remove(struct udevice *dev)
{
	struct tkey_sandbox_priv *priv = dev_get_priv(dev);

	if (priv->fd >= 0) {
		os_close(priv->fd);
		priv->fd = -1;
	}

	if (priv->path) {
		free(priv->path);
		priv->path = NULL;
	}
	log_debug("Disconnected\n");

	return 0;
}

/* TKey uclass operations */
static const struct tkey_ops tkey_sandbox_ops = {
	.read = tkey_sandbox_read,
	.write = tkey_sandbox_write,
};

static const struct udevice_id tkey_sandbox_ids[] = {
	{ .compatible = "sandbox,tkey" },
	{ }
};

U_BOOT_DRIVER(tkey_sandbox) = {
	.name		= "tkey_sandbox",
	.id		= UCLASS_TKEY,
	.of_match	= tkey_sandbox_ids,
	.probe		= tkey_sandbox_probe,
	.remove		= tkey_sandbox_remove,
	.ops		= &tkey_sandbox_ops,
	.priv_auto	= sizeof(struct tkey_sandbox_priv),
};
