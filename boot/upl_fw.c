// SPDX-License-Identifier: GPL-2.0
/*
 * Universal Payload (UPL) loading firmware phases
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY LOGC_BOOT

#include <bootdev.h>
#include <spl.h>

static int upl_load_from_image(struct spl_image_info *spl_image,
			       struct spl_boot_device *bootdev)
{
	printf("upl\n");

	return 0;
}
SPL_LOAD_IMAGE_METHOD("upl", 4, BOOT_DEVICE_UPL, upl_load_from_image);
