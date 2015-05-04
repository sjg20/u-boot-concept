/*
 * Copyright (c) 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

/* Temporary SPI implementation */

#include <common.h>
#include <spi_flash.h>

struct spi_flash *spi_flash_probe_fdt(const void *blob, int slave_node,
				      int spi_node)
{
	return spi_flash_probe(0, 0, 20000000, 0);
}
