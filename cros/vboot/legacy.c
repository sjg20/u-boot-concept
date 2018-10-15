/*
 * Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#include <common.h>
#include <os.h>
#include <cros/vboot.h>


int VbExLegacy(int altfw_num)
{
	printf("Legacy boot %d\n", altfw_num);
	return 1;
}

VbError_t VbExGetAltFWList(VbAltFwItem **item, uint32_t *count)
{
	void *u_boot, *tianocore;
	int u_boot_size, tianocore_size;

	if (os_read_file("/home/sglass/u/tools/logos/u-boot_logo_rgb.bmp", &u_boot, &u_boot_size))
		return VBERROR_UNKNOWN;
	if (os_read_file("/home/sglass/u/cros/data/tianocore.bmp", &tianocore, &tianocore_size))
		return VBERROR_UNKNOWN;

	/* TODO(sjg@chromium.org): Read from SPI flash */
	static VbAltFwItem items[] = {
		{ 1, "u-boot.bin", "U-Boot", "U-Boot Boot Loader v2018.09" },
		{ 2, "tianocore.bin", "TianoCore", "TianoCore v3.32" },
	};
	items[0].image = u_boot;
	items[0].image_size = u_boot_size;
	items[1].image = tianocore;
	items[1].image_size = tianocore_size;

	*count = ARRAY_SIZE(items);
	*item = items;

	return 0;
}
