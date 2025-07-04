// SPDX-License-Identifier: GPL-2.0
/*
 * Generation of tables for particular device types
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <dm.h>
#include <video.h>
#include <acpi/acpi_table.h>

int acpi_write_bgrt(struct acpi_ctx *ctx)
{
	struct video_priv *priv;
	struct udevice *dev;
	struct acpi_bgrt *bgrt;
	ulong width, height;
	void *logo;
	uint bpix;
	int size;

	/* The BGRT is only useful if video and a logo are enabled. */
	if (!CONFIG_IS_ENABLED(VIDEO) || !CONFIG_IS_ENABLED(VIDEO_LOGO))
		return -ENOTSUPP;

	if (uclass_first_device_err(UCLASS_VIDEO, &dev))
		return -EIO;

	priv = dev_get_uclass_priv(dev);
	logo = video_get_u_boot_logo(&size);

	/* If there's no logo data, there's nothing to report */
	if (!logo)
		return -ENOENT;

	video_bmp_get_info(logo, &width, &height, &bpix);

	bgrt = ctx->current;
	ctx->tab_start = ctx->current;
	memset(bgrt, '\0', sizeof(struct acpi_table_iort));

	acpi_fill_header(&bgrt->header, "BGRT");
	bgrt->version = 1;

	/* Status: Bit 0 (Displayed) = 1, Bits 1-2 (Orientation) = 0 */
	bgrt->status = 1;

	/* Image Type: 0 = Bitmap */
	bgrt->image_type = 0;

	/* The physical address of the in-memory logo bitmap */
	bgrt->addr = (ulong)logo;

	/* Calculate offsets to center the logo on the screen */
	if (priv->xsize > width)
		bgrt->offset_x = (priv->xsize - width) / 2;
	else
		bgrt->offset_x = 0;

	if (priv->ysize > height)
		bgrt->offset_y = (priv->ysize - height) / 2;
	else
		bgrt->offset_y = 0;

	/* Calculate length and checksum */
	acpi_update_checksum(&bgrt->header);
	log_debug("BGRT at %p, length %x\n", bgrt, bgrt->header.length);
	acpi_add_table(ctx, bgrt);

	return 0;
}
