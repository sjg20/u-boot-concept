// SPDX-License-Identifier: GPL-2.0+
/*
 * EFI input key decoding functions
 *
 * Copyright (c) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY	LOGC_EFI

#include <efi.h>
#include <efi_api.h>
#include <cli.h>

int efi_decode_key(struct efi_input_key *key)
{
	static const char conv_scan[] = {0, 'p', 'n', 'f', 'b', 'a', 'e', 0, 8};
	int ch;

	ch = key->unicode_char;

	/*
	 * Unicode char 8 (for backspace) is never returned. Instead we get a
	 * key scan code of 8. Handle this so that backspace works correctly
	 * in the U-Boot command line.
	 */
	if (!ch && key->scan_code < sizeof(conv_scan)) {
		ch = conv_scan[key->scan_code];
		if (ch >= 'a')
			ch -= 'a' - 1;
	}
	log_debug(" [%x %x %x] ", ch, key->unicode_char, key->scan_code);

	return ch;
}

int efi_decode_key_ex(struct efi_key_data *key_data)
{
	return efi_decode_key(&key_data->key);
}
