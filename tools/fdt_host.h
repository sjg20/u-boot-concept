/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2008 Semihalf
 */

#ifndef __FDT_HOST_H__
#define __FDT_HOST_H__

/* Make sure to include u-boot version of libfdt include files */
#include "../include/linux/libfdt.h"
#include "../include/fdt_support.h"

struct image_region;

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
 * fit_check_sign() - Check a signature in a FIT
 *
 * @fit: FIT to check
 * @key: Key FDT blob to check against
 * @fit_uname_config: Name of configuration to check (NULL for default)
 * @return 0 if OK, -ve if signature failed
 */
int fit_check_sign(const void *fit, const void *key,
		   const char *fit_uname_config);

/**
 * fdt_get_regions() - Get the regions to sign
 *
 * This calculates a list of node to hash for this particular configuration,
 * then finds which regions of the devicetree they correspond to.
 *
 * @blob:	Pointer to the FDT blob to sign
 * @strtab_len:	Length in bytes of the string table to sign, -1 to sign it all
 * @regionp: Returns list of regions that need to be hashed (allocated; must be
 *	freed by the caller)
 * @region_count: Returns number of regions
 * @return 0 if OK, -ENOMEM if out of memory, -EIO if the regions to hash could
 * not be found, -EINVAL if no registers were found to hash
 */
int fdt_get_regions(const void *blob, int strtab_len,
		    struct image_region **regionp, int *region_countp);

#endif /* __FDT_HOST_H__ */
