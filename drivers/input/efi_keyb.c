// SPDX-License-Identifier: GPL-2.0+
/*
 * EFI-keyboard input driver
 *
 * Uses EFI's Simple Text Input Protocol, polling keystrokes and providing them
 * to stdio
 */

#include <efi_loader.h>
#include <efi.h>
#include <log.h>
#include <keyboard.h>
#include <stdio_dev.h>
#include <dm.h>
#include <linux/delay.h>

struct efi_kbd_priv {
	struct efi_simple_text_input_ex_protocol *ex_con;
	struct efi_simple_text_input_protocol *con_in;
	struct efi_input_key key_buffer;
	struct efi_key_data exkey;

	bool have_key;
};

/**
 * efi_kbd_tstc() - Test for a character from EFI
 *
 * @dev: keyboard device
 * Return: 1 if a character is available, 0 otherwise.
 */
static int efi_kbd_tstc(struct udevice *dev)
{
	struct efi_kbd_priv *priv = dev_get_priv(dev);
	efi_status_t status;

	mdelay(10);
	printf(".");

	/* If we already have a key from a previous check, report it's available */
	if (priv->have_key)
		return 1;

	/* wait until we don't see EFI_NOT_READY */
	if (priv->ex_con) {
		printf("$");
		status = priv->ex_con->read_key_stroke_ex(priv->ex_con,
							&priv->exkey);
	} else {
		printf("#");
		status = priv->con_in->read_key_stroke(priv->con_in,
							&priv->key_buffer);
	}
	if (!status)
		printf("y ");
	// else
		// printf("n ");
	if (status == EFI_SUCCESS) {
		priv->have_key = true;
		return 1;
	}

	return 0;
}

/**
 * efi_kbd_getc() - Get a character from EFI
 *
 * Waits until a key is available and returns the associated character
 *
 * @dev: stdio device pointer
 * Return: character code
 */
static int efi_kbd_getc(struct udevice *dev)
{
	struct efi_kbd_priv *priv = dev_get_priv(dev);

	printf("getc\n");

	while (!efi_kbd_tstc(dev))
		udelay(100);

	priv->have_key = false;
	if (priv->ex_con) {
		struct efi_key_data *exkey = &priv->exkey;

		printf("got exkey %x\n", exkey->key.unicode_char);
		if (exkey->key.unicode_char)
			return exkey->key.unicode_char;
	} else {
		struct efi_input_key *key = &priv->key_buffer;

		printf("got key %x\n", key->unicode_char);

		/* translate the EFI_INPUT_KEY to a character */
		if (key->unicode_char) {
			/* EFI uses '\r' for Enter; we expect '\n' */
			if (key->unicode_char == '\r')
				return '\n';
			return key->unicode_char;
		}
	}
#if 0
	switch (key->scan_code) {
	case '\b':
	case '\e':
		return key->scan_code;
	default:
		/* TODO: deal with arrow keys, etc. */
		return 0;
	}

#endif
	return 0;
}

/**
 * efi_kbd_start() - Start the driver
 *
 * Reset the keyboard ready for use
 *
 * Return: 0 on success (always)
 */
static int efi_kbd_start(struct udevice *dev)
{
	struct efi_kbd_priv *priv = dev_get_priv(dev);

	log_info("efi_kbd_start\n");

	/* reset keyboard to drop anything pressed during UEFI startup */
	priv->con_in->reset(priv->con_in, true);
	priv->have_key = false;
	log_info("reset done\n");

	return 0;
}

static int efi_kbd_probe(struct udevice *dev)
{
	struct keyboard_priv *uc_priv = dev_get_uclass_priv(dev);

	// struct efi_boot_services *boot = efi_get_boot();
	struct efi_system_table *systab = efi_get_sys_table();
	struct stdio_dev *sdev = &uc_priv->sdev;
	struct efi_kbd_priv *priv = dev_get_priv(dev);
	// efi_status_t eret;
	int ret;
	// const efi_guid_t guid = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;

	log_info("keyboard probe '%s'\n", dev->name);

#if 0
	eret = boot->open_protocol(systab->con_in_handle, &guid,
				   (void **)&priv->ex_con, NULL, NULL,
				   EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	printf("eret %lx\n", eret);
#endif

	priv->con_in = systab->con_in;
	if (!priv->con_in) {
		log_err("EFI: Simple Text Input Protocol not found\n");
		return -ENODEV;
	}

	strcpy(sdev->name, "efi-kbd");
	ret = input_stdio_register(sdev);
	if (ret) {
		log_err("input_stdio_register() failed\n");
		return ret;
	}

	return 0;
}

static const struct keyboard_ops efi_kbd_ops = {
	.start	= efi_kbd_start,
	.tstc	= efi_kbd_tstc,
	.getc	= efi_kbd_getc,
};

static const struct udevice_id efi_kbd_ids[] = {
	{ .compatible = "efi-keyboard" },
	{ }
};

U_BOOT_DRIVER(efi_kbd) = {
	.name		= "efi_kbd",
	.id		= UCLASS_KEYBOARD,
	.of_match	= efi_kbd_ids,
	.ops		= &efi_kbd_ops,
	.priv_auto	= sizeof(struct efi_kbd_priv),
	.probe		= efi_kbd_probe,
};
