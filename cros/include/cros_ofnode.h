/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#ifndef CROS_FDTDEC_H_
#define CROS_FDTDEC_H_

#include <fdtdec.h>
#include <cros/fmap.h>
#include <ec_commands.h>

/* Decode Chrome OS specific configuration from fdt */

int cros_ofnode_flashmap(struct twostop_fmap *config);

/**
 * Return the /chromeos-config ofnode
 *
 * @return ofnode found, of ofnode_null() if not found
 */
ofnode cros_ofnode_config_node(void);

/**
 * This checks whether a property exists.
 *
 * @param fdt	FDT blob to use
 * @param name	The path and name to the property in question
 * @return non-zero if the property exists, zero if it does not exist.
 */
int cros_ofnode_config_has_prop(const void *fdt, const char *name);

/**
 * Decode a named region within a memory bank of a given type.
 *
 * The properties are looked up in the /chromeos-config node/
 *
 * See fdtdec_decode_memory_region() for more details.
 *
 * @param mem_type	Type of memory to use, which is a name, such as
 *			"u-boot" or "kernel".
 * @param suffix	String to append to the memory/offset
 *			property names
 * @param basep		Returns base of region
 * @param sizep		Returns size of region
 * @return pointer to region, or NULL if property not found/malloc failed
 */
int cros_ofnode_decode_region(const char *mem_type, const char *suffix,
			      fdt_addr_t *basep, fdt_size_t *sizep);

/**
 * Returns information from the FDT about memory for a given root
 *
 * @param name          Root name of alias to search for
 * @param config        structure to use to return information
 */
int cros_ofnode_memory(const char *name, struct fdt_memory *config);

int cros_ofnode_find_locale(const char *name, struct fmap_entry *entry);
#endif /* CROS_FDTDEC_H_ */
