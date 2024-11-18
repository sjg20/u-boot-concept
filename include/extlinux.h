/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __extlinux_h
#define __extlinux_h

#include <pxe_utils.h>

#define EXTLINUX_FNAME	"extlinux/extlinux.conf"

/**
 * struct extlinux_info - useful information for extlinux_getfile()
 *
 * @dev: bootmethod device being used to boot
 * @bflow: bootflow being booted
 */
struct extlinux_info {
	struct udevice *dev;
	struct bootflow *bflow;
};

/**
 * struct extlinux_plat - locate state for this bootmeth
 *
 * @use_falllback: true to boot with the fallback option
 * @ctx: holds the PXE context, if it should be saved
 * @info: information used for the getfile() method
 */
struct extlinux_plat {
	bool use_fallback;
	struct pxe_context ctx;
	struct extlinux_info info;
};

enum extlinux_option_type {
	EO_FALLBACK,
	EO_INVALID
};

struct extlinux_option {
	char *name;
	enum extlinux_option_type option;
};

enum extlinux_option_type extlinux_get_option(const char *option);

int extlinux_set_property(struct udevice *dev, const char *property,
			  const char *value);

#endif
