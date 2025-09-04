// SPDX-License-Identifier: GPL-2.0+
/*
 * Simplified U-Boot library interface implementation
 *
 * Copyright 2025 Canonical
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#include <string.h>
#include <version.h>
#include <asm/global_data.h>
#include <u-boot.h>

/* Static storage for global data when using simplified API */
static struct global_data static_gd;

int ulib_init(char *progname)
{
	int ret;

	/* Initialize the U-Boot library with our static global data */
	ret = ulib_init_with_data(progname, &static_gd);
	if (ret)
		return ret;

	return 0;
}

void ulib_uninit(void)
{
}

const char *ulib_get_version(void)
{
	return PLAIN_VERSION;
}
