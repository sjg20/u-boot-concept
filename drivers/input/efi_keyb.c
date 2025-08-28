// SPDX-License-Identifier: GPL-2.0+
/*
 * EFI-keyboard input driver
 *
 * Uses EFI's Simple Text Input Protocol, polling keystrokes and providing them
 * to stdio
 */

#define LOG_CATEGORY	LOGC_EFI

#include <dm.h>
#include <efi_loader.h>
#include <efi.h>
#include <keyboard.h>
#include <log.h>
#include <stdio_dev.h>
#include <linux/delay.h>

/*
 * struct efi_kbd_priv - private information for the keyboard
 *
 * @ex_con
 */
struct efi_kbd_priv {
	struct efi_simple_text_input_ex_protocol *ex_con;
	struct efi_simple_text_input_protocol *con_in;
	struct efi_input_key key;
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

	/* If we already have a key from a previous check, report it's available */
	if (priv->have_key)
		return 1;

	/* wait until we don't see EFI_NOT_READY */
	if (priv->ex_con) {
		status = priv->ex_con->read_key_stroke_ex(priv->ex_con,
							  &priv->exkey);
	} else {
		status = priv->con_in->read_key_stroke(priv->con_in,
						       &priv->key);
	}
	if (!status) {
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
 * Return: character code, or 0 if none
 */
static int efi_kbd_getc(struct udevice *dev)
{
	struct efi_kbd_priv *priv = dev_get_priv(dev);

	if (!efi_kbd_tstc(dev))
		return 0;

	priv->have_key = false;
	if (priv->ex_con) {
		struct efi_input_key *exkey = &priv->exkey.key;

		log_debug("got exkey %x scan %x\n", exkey->unicode_char,
			  exkey->scan_code);
		return efi_decode_key(exkey);
	} else {
		struct efi_input_key *key = &priv->key;

		log_debug("got key %x\n", key->unicode_char);
		return efi_decode_key(key);
	}

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

	log_debug("keyboard start\n");

	/* reset keyboard to drop anything pressed during UEFI startup */
	priv->con_in->reset(priv->con_in, true);
	if (priv->ex_con)
		priv->ex_con->reset(priv->ex_con, true);
	priv->have_key = false;

	return 0;
}

static int efi_kbd_probe(struct udevice *dev)
{
	struct keyboard_priv *uc_priv = dev_get_uclass_priv(dev);
	struct efi_system_table *systab = efi_get_sys_table();
	struct stdio_dev *sdev = &uc_priv->sdev;
	struct efi_kbd_priv *priv = dev_get_priv(dev);
	efi_status_t ret_efi;
	int ret;

	log_debug("keyboard probe '%s'\n", dev->name);
	priv->con_in = systab->con_in;

	/* Try to get the EFI Simple Text Input EX protocol from console handle */
	if (systab->con_in_handle) {
		efi_guid_t ex_guid = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;

		ret_efi = efi_get_boot()->open_protocol(systab->con_in_handle,
						&ex_guid,
						(void **)&priv->ex_con,
						NULL, NULL,
						EFI_OPEN_PROTOCOL_GET_PROTOCOL);
		if (ret_efi != EFI_SUCCESS) {
			log_debug("Extended input protocol not available\n");
			priv->ex_con = NULL;
		}
	}

	strcpy(sdev->name, "efi-kbd");
	ret = input_stdio_register(sdev);
	if (ret) {
		log_err("Failed to register\n");
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
