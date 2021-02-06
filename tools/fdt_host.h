/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2008 Semihalf
 */

#ifndef __FDT_HOST_H__
#define __FDT_HOST_H__

/* Make sure to include u-boot version of libfdt include files */
#include "../include/linux/libfdt.h"
#include "../include/fdt_support.h"

/**
 * fdt_remove_unused_strings() - Remove any unused strings from an FDT
 *
 * This creates a new device tree in @new with unused strings removed. The
 * called can then use fdt_pack() to minimise the space consumed.
 *
 * @old:	Old device tree blog
 * @new:	Place to put new device tree blob, which must be as large as
 *		@old
 * @return
 *	0, on success
 *	-FDT_ERR_BADOFFSET, corrupt device tree
 *	-FDT_ERR_NOSPACE, out of space, which should not happen unless there
 *		is something very wrong with the device tree input
 */
int fdt_remove_unused_strings(const void *old, void *new);

/**
 * fit_check_sign() - Check the signatures in an image
 *
 * This verified the configuration signature and then loads all the images it
 * contains
 *
 * @fit: FIT image to load
 * @size: Size of FIT image in bytes
 * @key: Key FDT blob to use (contains public key)
 * @key_size: Size of key FDT blob
 * @fit_uname_config: Name of configuration to verify
 * @return 0 if OK, -ve on error
 */
int fit_check_sign(const void *fit, ulong size, const void *key, ulong key_size,
		   const char *fit_uname_config);

#endif /* __FDT_HOST_H__ */
