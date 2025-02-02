// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2025, Simon Glass <simon.glass@canonical.com>
 */

#include <errno.h>
#include <log.h>
#include <vesa.h>
#include <video.h>

int vesa_setup_video_priv(struct vesa_mode_info *vesa, u64 fb,
			  struct video_priv *uc_priv,
			  struct video_uc_plat *plat)
{
	if (!vesa->x_resolution)
		return log_msg_ret("No x resolution", -ENXIO);
	uc_priv->xsize = vesa->x_resolution;
	uc_priv->ysize = vesa->y_resolution;
	uc_priv->line_length = vesa->bytes_per_scanline;
	switch (vesa->bits_per_pixel) {
	case 32:
	case 24:
		uc_priv->bpix = VIDEO_BPP32;
		break;
	case 16:
		uc_priv->bpix = VIDEO_BPP16;
		break;
	default:
		return -EPROTONOSUPPORT;
	}

	/* Use double buffering if enabled */
	if (IS_ENABLED(CONFIG_VIDEO_COPY) && plat->base)
		plat->copy_base = fb;
	else
		plat->base = fb;
	log_debug("base = %lx, copy_base = %lx\n", plat->base, plat->copy_base);
	plat->size = vesa->bytes_per_scanline * vesa->y_resolution;

	return 0;
}
