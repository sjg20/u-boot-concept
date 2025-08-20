// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2020, Linaro Limited
 */

#define LOG_CATEGORY LOGC_EFI

#include <efi.h>
#include <efi_device_path.h>
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

/**
 * efi_load_option_dp_join() - join device-paths for load option
 *
 * @dp:		in: binary device-path, out: joined device-path
 * @dp_size:	size of joined device-path
 * @initrd_dp:	initrd device-path or NULL
 * @fdt_dp:	device-tree device-path or NULL
 * Return:	status_code
 */
efi_status_t efi_load_option_dp_join(struct efi_device_path **dp,
				     size_t *dp_size,
				     struct efi_device_path *initrd_dp,
				     struct efi_device_path *fdt_dp)
{
	if (!dp)
		return EFI_INVALID_PARAMETER;

	*dp_size = efi_dp_size(*dp);

	if (initrd_dp) {
		struct efi_device_path *tmp_dp = *dp;

		*dp = efi_dp_concat(tmp_dp, initrd_dp, *dp_size);
		efi_free_pool(tmp_dp);
		if (!*dp)
			return EFI_OUT_OF_RESOURCES;
		*dp_size += efi_dp_size(initrd_dp) + sizeof(EFI_DP_END);
	}

	if (fdt_dp) {
		struct efi_device_path *tmp_dp = *dp;

		*dp = efi_dp_concat(tmp_dp, fdt_dp, *dp_size);
		efi_free_pool(tmp_dp);
		if (!*dp)
			return EFI_OUT_OF_RESOURCES;
		*dp_size += efi_dp_size(fdt_dp) + sizeof(EFI_DP_END);
	}

	*dp_size += sizeof(EFI_DP_END);

	return EFI_SUCCESS;
}
