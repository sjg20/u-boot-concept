// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2020, Linaro Limited
 */

#define LOG_CATEGORY LOGC_EFI

#include <string.h>
#include <linux/types.h>

static int u16_tohex(u16 c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;

	/* not hexadecimal */
	return -1;
}

bool efi_varname_is_load_option(u16 *var_name16, int *index)
{
	int id, i, digit;

	if (memcmp(var_name16, u"Boot", 8))
		return false;

	for (id = 0, i = 0; i < 4; i++) {
		digit = u16_tohex(var_name16[4 + i]);
		if (digit < 0)
			break;
		id = (id << 4) + digit;
	}
	if (i == 4 && !var_name16[8]) {
		if (index)
			*index = id;
		return true;
	}

	return false;
}
