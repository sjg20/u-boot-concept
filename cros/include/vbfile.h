/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Interface for accessing files in SPI flash
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __CROS_VBFILE_H
#define __CROS_VBFILE_H

struct vboot_info;

int vbfile_load(struct vboot_info *vboot, const char *name,
		u8 **imagep, int *image_sizep);

#endif /* __CROS_VBFILE_H */
