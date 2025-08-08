// SPDX-License-Identifier: GPL-2.0+
/*
 * EFI Keyboard input driver for U-Boot
 *
 * This driver acts as a client to the host UEFI firmware's Simple Text
 * Input Protocol. It polls for keystrokes from the host and provides them
 * to the U-Boot console subsystem, allowing the command line to function
 * when U-Boot is run as an EFI application.
 *
 * Copyright (c) 2025 Your Name
 */

#include <efi_loader.h>
#include <efi.h>
#include <log.h>
#include <stdio_dev.h>
#include <dm.h>
#include <linux/delay.h>

/* Store the pointer to the host UEFI's input protocol */
static struct efi_simple_text_input_protocol *host_con_in;

/* A single-key buffer to bridge between non-blocking checks and blocking reads */
static struct efi_input_key key_buffer;
static bool key_is_buffered;

/**
 * efi_kbd_tstc() - Test for a character from the EFI host
 *
 * This function is the U-Boot 'tstc' (test character) implementation. It
 * performs a non-blocking check to see if a key has been pressed by calling
 * the host UEFI firmware's ReadKeyStroke service.
 *
 * @dev: stdio device pointer
 * @return: 1 if a character is available, 0 otherwise.
 */
static int efi_kbd_tstc(struct stdio_dev *dev)
{
	efi_status_t status;

	/* If we already have a key from a previous check, report it's available */
	if (key_is_buffered)
		return 1;

	/*
	 * Poll the host's protocol. EFI_NOT_READY is the expected status
	 * when no key is waiting. Any other error is treated as no key.
	 */
	status = EFI_CALL(host_con_in->read_key_stroke(host_con_in, &key_buffer));
	if (status == EFI_SUCCESS) {
		key_is_buffered = true;
		return 1;
	}

	return 0;
}

/**
 * efi_kbd_getc() - Get a character from the EFI host
 *
 * This function is the U-Boot 'getc' (get character) implementation. It
 * waits until a key is available and returns its character representation.
 *
 * @dev: stdio device pointer
 * @return: The character code for the pressed key.
 */
static int efi_kbd_getc(struct stdio_dev *dev)
{
	/* Wait until tstc() reports a key is ready */
	while (!efi_kbd_tstc(dev))
		/* This loop provides a small delay to prevent busy-spinning */
		udelay(100);

	/* Mark the buffer as consumed */
	key_is_buffered = false;

	/*
	 * Translate the EFI_INPUT_KEY to a single character.
	 * The UnicodeChar is preferred. If it's zero, we map a few
	 * common scan codes to their ASCII equivalents.
	 */
	if (key_buffer.unicode_char != 0) {
		/* EFI uses '\r' for Enter, U-Boot console expects '\n' */
		if (key_buffer.unicode_char == '\r')
			return '\n';
		return key_buffer.unicode_char;
	}

	/* Handle important non-Unicode keys */
	switch (key_buffer.scan_code) {
	case SCAN_CODE_BACKSPACE:
		return '\b'; /* Backspace */
	case SCAN_CODE_ESC:
		return 0x1b; /* Escape */
	default:
		/* Ignore other special keys (arrows, function keys, etc.) */
		return 0;
	}
}

/**
 * efi_keyboard_init() - Initialize the EFI keyboard driver
 *
 * This function locates the host UEFI's input protocol and registers a new
 * U-Boot stdio device to handle keyboard input.
 *
 * @return: 0 on success, -1 on failure.
 */
int efi_keyboard_init(void)
{
	/* This driver is only useful when U-Boot is loaded via EFI */
	if (!efi_is_loaded())
		return -1;

	log_debug("Initializing EFI keyboard driver...\n");

	/* The host input protocol is in the EFI System Table (ConIn) */
	host_con_in = efi_st->ConIn;
	if (!host_con_in) {
		log_err("EFI: Host Simple Text Input Protocol not found!\n");
		return -1;
	}

	/*
	 * Define the new stdio device that uses our EFI-aware functions.
	 */
	static struct stdio_dev efi_kbd_dev = {
		.name	= "efi_kbd",
		.flags	= DEV_FLAGS_INPUT,
		.getc	= efi_kbd_getc,
		.tstc	= efi_kbd_tstc,
	};

	/* Register the device with U-Boot's driver model */
	if (stdio_register(&efi_kbd_dev)) {
		log_err("EFI: Failed to register keyboard stdio device!\n");
		return -1;
	}

	/*
	 * Set stdin and stderr to our new device. This makes the U-Boot
	 * command line use this driver for input. stderr is aliased to
	 * allow for cancellation of operations with Ctrl+C.
	 */
	env_set("stdin", "efi_kbd");
	env_set("stderr", "efi_kbd");

	log_info("EFI keyboard driver registered as 'efi_kbd'.\n");

	/*
	 * The EFI spec requires we empty the keyboard buffer after taking
	 * control to discard any key presses made during UEFI's boot process.
	 */
	EFI_CALL(host_con_in->reset(host_con_in, false));
	key_is_buffered = false;

	return 0;
}
