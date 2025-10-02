// SPDX-License-Identifier: GPL-2.0
/*
 * Generation of tables for particular device types
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY	LOGC_ACPI

#include <dm.h>
#include <efi_loader.h>
#include <mapmem.h>
#include <video.h>
#include <acpi/acpi_table.h>

int acpi_write_bgrt(struct acpi_ctx *ctx)
{
	struct udevice *dev;
	struct acpi_bgrt *bgrt;
	efi_status_t eret;
	void *logo, *buf;
	bool have_video;
	int size;

	/* If video is available, use the screen size to centre the logo */
	have_video = !uclass_first_device_err(UCLASS_VIDEO, &dev);

	if (!IS_ENABLED(CONFIG_VIDEO))
		return -ENOENT;

	logo = video_image_get(bgrt, &size);

	/* If there's no logo data, there's nothing to report */
	if (!logo)
		return -ENOENT;

	bgrt = ctx->current;
	ctx->tab_start = ctx->current;
	memset(bgrt, '\0', sizeof(struct acpi_table_iort));

	acpi_fill_header(&bgrt->header, "BGRT");
	bgrt->version = 1;

	/* Status: Bit 0 (Displayed) = 1, Bits 1-2 (Orientation) = 0 */
	bgrt->status = 1;

	/* Image Type: 0 = Bitmap */
	bgrt->image_type = 0;

	/* Mark space used for tables */
	eret = efi_allocate_pool(EFI_BOOT_SERVICES_DATA, size, &buf);
	if (eret)
		return -ENOMEM;
	memcpy(buf, logo, size);

	/* The physical address of the in-memory logo bitmap */
	bgrt->addr = nomap_to_sysmem(buf);

	/* Calculate offsets to center the logo on the screen */
	bgrt->offset_x = 0;
	bgrt->offset_y = 0;

	/*
	 * centering is disabled for now, since it seems to be done by the
	 * startup code
	 */
	if (0 && IS_ENABLED(CONFIG_VIDEO) && have_video) {
		struct video_priv *priv = dev_get_uclass_priv(dev);
		ulong width, height;
		uint bpix;

		video_bmp_get_info(logo, &width, &height, &bpix);

		if (priv->xsize > width)
			bgrt->offset_x = (priv->xsize - width) / 2;
		if (priv->ysize > height)
			bgrt->offset_y = (priv->ysize - height) / 2;
	}
	acpi_inc(ctx, sizeof(*bgrt));

	/* Calculate length and checksum */
	bgrt->header.length = (ulong)ctx->current - (ulong)bgrt;
	acpi_update_checksum(&bgrt->header);
	log_debug("BGRT at %p length %x logo copied to bs-data at %p\n", bgrt,
		  bgrt->header.length, buf);
	acpi_add_table(ctx, bgrt);

	return 0;
}
