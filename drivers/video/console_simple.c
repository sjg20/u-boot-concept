// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2015 Google, Inc
 * (C) Copyright 2015
 * Bernecker & Rainer Industrieelektronik GmbH - http://www.br-automation.com
 * (C) Copyright 2023 Dzmitry Sankouski <dsankouski@gmail.com>
 */

#include <common.h>
#include <dm.h>
#include <video.h>
#include <video_console.h>
#include <video_font.h>		/* Get font data, width and height */

#define FLIPPED_DIRECTION 1
#define NORMAL_DIRECTION 0

/**
 * struct console_simple_priv - Private data for this driver
 *
 * @video_fontdata	font graphical representation data
 */
struct console_simple_priv {
	struct video_fontdata *fontdata;
};

/**
 * console_set_font() - prepare vidconsole for chosen font.
 *
 * @dev		vidconsole device
 * @fontdata	pointer to font data struct
 */
static int console_set_font(struct udevice *dev, struct video_fontdata *fontdata)
{
	struct console_simple_priv *priv = dev_get_priv(dev);
	struct vidconsole_priv *vc_priv = dev_get_uclass_priv(dev);
	struct video_priv *vid_priv = dev_get_uclass_priv(dev->parent);

	debug("console_simple: setting %s font\n", fontdata->name);
	debug("width: %d\n", fontdata->width);
	debug("byte width: %d\n", fontdata->byte_width);
	debug("height: %d\n", fontdata->height);

	priv->fontdata = fontdata;
	vc_priv->x_charsize = fontdata->width;
	vc_priv->y_charsize = fontdata->height;
	if (vid_priv->rot % 2) {
		vc_priv->cols = vid_priv->ysize / fontdata->width;
		vc_priv->rows = vid_priv->xsize / fontdata->height;
		vc_priv->xsize_frac = VID_TO_POS(vid_priv->ysize);
	} else {
		vc_priv->cols = vid_priv->xsize / fontdata->width;
		vc_priv->rows = vid_priv->ysize / fontdata->height;
	}

	return 0;
}

/**
 * Checks if bits per pixel supported.
 *
 * @param bpix	framebuffer bits per pixel.
 *
 * @returns 0, if supported, or else -ENOSYS.
 */
static int check_bpix_support(int bpix)
{
	switch (bpix) {
	case VIDEO_BPP8:
		if (IS_ENABLED(CONFIG_VIDEO_BPP8))
			return 0;
	case VIDEO_BPP16:
		if (IS_ENABLED(CONFIG_VIDEO_BPP16))
			return 0;
	case VIDEO_BPP32:
		if (IS_ENABLED(CONFIG_VIDEO_BPP32))
			return 0;
	default:
		return -ENOSYS;
	}
}

/**
 * Fill 1 pixel in framebuffer, and go to next one.
 *
 * @param dstp		a pointer to pointer to framebuffer.
 * @param value		value to write to framebuffer.
 * @param pbytes	framebuffer bytes per pixel.
 * @param step		framebuffer pointer increment. Usually is equal to pbytes,
 *			and may be negative to control filling direction.
 */
static inline void fill_pixel_and_goto_next(void **dstp, u32 value, int pbytes, int step)
{
	u8 *dst_byte = *dstp;

	if (pbytes == 4) {
		u32 *dst = *dstp;
		*dst = value;
	}
	if (pbytes == 2) {
		u16 *dst = *dstp;
		*dst = value;
	}
	if (pbytes == 1) {
		u8 *dst = *dstp;
		*dst = value;
	}
	*dstp = dst_byte + step;
}

#if (CONFIG_IS_ENABLED(CONSOLE_ROTATION))
/**
 * Fills 1 character in framebuffer horizontally.
 * Horizontally means we're filling char font data columns across the lines.
 *
 * @param pfont		a pointer to character font data.
 * @param line		a pointer to pointer to framebuffer. It's a point for upper left char corner
 * @param vid_priv	driver private data.
 * @fontdata		font graphical representation data
 * @param direction	controls character orientation. Can be normal or flipped.
 * When normal:               When flipped:
 *|-----------------------------------------------|
 *|               *        |   line stepping      |
 *|    ^  * * * * *        |   |                  |
 *|    |    *     *        |   v   *     *        |
 *|    |                   |       * * * * *      |
 *|  line stepping         |       *              |
 *|                        |                      |
 *|  stepping ->           |        <- stepping   |
 *|---!!we're starting from upper left char corner|
 *|-----------------------------------------------|
 *
 * @returns 0, if success, or else error code.
 */
static int fill_char_horizontally(uchar *pfont, void **line, struct video_priv *vid_priv,
				  struct video_fontdata *fontdata, bool direction)
{
	int step, line_step, pbytes, bitcount = 8, width_remainder, ret;
	void *dst;
	u8 mask;

	ret = check_bpix_support(vid_priv->bpix);
	if (ret)
		return ret;

	pbytes = VNBYTES(vid_priv->bpix);
	if (direction) {
		step = -pbytes;
		line_step = vid_priv->line_length;
	} else {
		step = pbytes;
		line_step = -vid_priv->line_length;
	}

	width_remainder = fontdata->width % 8;
	for (int col = 0; col < fontdata->byte_width; col++) {
		mask = 0x80;
		if (width_remainder) {
			bool is_last_col = (fontdata->byte_width - col == 1);

			if (is_last_col)
				bitcount = width_remainder;
		}
		for (int bit = 0; bit < bitcount; bit++) {
			dst = *line;
			for (int row = 0; row < fontdata->height; row++) {
				u32 value = (pfont[row * fontdata->byte_width + col]
					     & mask) ? vid_priv->colour_fg : vid_priv->colour_bg;

				fill_pixel_and_goto_next(&dst,
							 value,
							 pbytes,
							 step
				);
			}
			*line += line_step;
			mask >>= 1;
		}
	}
	return ret;
}
#endif

/**
 * Fills 1 character in framebuffer vertically. Vertically means we're filling char font data rows
 * across the lines.
 *
 * @param pfont		a pointer to character font data.
 * @param line		a pointer to pointer to framebuffer. It's a point for upper left char corner
 * @param vid_priv	driver private data.
 * @fontdata		font graphical representation data
 * @param direction	controls character orientation. Can be normal or flipped.
 * When normal:               When flipped:
 *|-----------------------------------------------|
 *| line stepping        |                        |
 *|            |         |       stepping ->      |
 *|     *      |         |       * * *            |
 *|   * *      v         |         *              |
 *|     *                |         *              |
 *|     *                |         * *      ^     |
 *|   * * *              |         *        |     |
 *|                      |                  |     |
 *| stepping ->          |         line stepping  |
 *|---!!we're starting from upper left char corner|
 *|-----------------------------------------------|
 *
 * @returns 0, if success, or else error code.
 */
static int fill_char_vertically(uchar *pfont, void **line, struct video_priv *vid_priv,
				struct video_fontdata *fontdata, bool direction)
{
	int step, line_step, pbytes, bitcount, width_remainder, ret;
	void *dst;

	ret = check_bpix_support(vid_priv->bpix);
	if (ret)
		return ret;

	pbytes = VNBYTES(vid_priv->bpix);
	if (direction) {
		step = -pbytes;
		line_step = -vid_priv->line_length;
	} else {
		step = pbytes;
		line_step = vid_priv->line_length;
	}

	width_remainder = fontdata->width % 8;
	for (int row = 0; row < fontdata->height; row++) {
		uchar bits;

		bitcount = 8;
		dst = *line;
		for (int col = 0; col < fontdata->byte_width; col++) {
			if (width_remainder) {
				bool is_last_col = (fontdata->byte_width - col == 1);

				if (is_last_col)
					bitcount = width_remainder;
			}
			bits = pfont[col];

			for (int bit = 0; bit < bitcount; bit++) {
				u32 value = (bits & 0x80) ?
					vid_priv->colour_fg :
					vid_priv->colour_bg;

				fill_pixel_and_goto_next(&dst,
							 value,
							 pbytes,
							 step
				);
				bits <<= 1;
			}
		}
		*line += line_step;
		pfont += fontdata->byte_width;
	}
	return ret;
}

static int console_set_row(struct udevice *dev, uint row, int clr)
{
	struct video_priv *vid_priv = dev_get_uclass_priv(dev->parent);
	struct console_simple_priv *priv = dev_get_priv(dev);
	struct video_fontdata *fontdata = priv->fontdata;
	void *line, *dst, *end;
	int pixels = fontdata->height * vid_priv->xsize;
	int ret;
	int i;
	int pbytes;

	ret = check_bpix_support(vid_priv->bpix);
	if (ret)
		return ret;

	line = vid_priv->fb + row * fontdata->height * vid_priv->line_length;
	dst = line;
	pbytes = VNBYTES(vid_priv->bpix);
	for (i = 0; i < pixels; i++)
		fill_pixel_and_goto_next(&dst, clr, pbytes, pbytes);
	end = dst;

	ret = vidconsole_sync_copy(dev, line, end);
	if (ret)
		return ret;

	return 0;
}

static int console_move_rows(struct udevice *dev, uint rowdst,
			     uint rowsrc, uint count)
{
	struct video_priv *vid_priv = dev_get_uclass_priv(dev->parent);
	struct console_simple_priv *priv = dev_get_priv(dev);
	struct video_fontdata *fontdata = priv->fontdata;
	void *dst;
	void *src;
	int size;
	int ret;

	dst = vid_priv->fb + rowdst * fontdata->height * vid_priv->line_length;
	src = vid_priv->fb + rowsrc * fontdata->height * vid_priv->line_length;
	size = fontdata->height * vid_priv->line_length * count;
	ret = vidconsole_memmove(dev, dst, src, size);
	if (ret)
		return ret;

	return 0;
}

static int console_putc_xy(struct udevice *dev, uint x_frac, uint y, char ch)
{
	struct vidconsole_priv *vc_priv = dev_get_uclass_priv(dev);
	struct udevice *vid = dev->parent;
	struct video_priv *vid_priv = dev_get_uclass_priv(vid);
	struct console_simple_priv *priv = dev_get_priv(dev);
	struct video_fontdata *fontdata = priv->fontdata;
	int pbytes = VNBYTES(vid_priv->bpix);
	int x, linenum, ret;
	void *start, *line;
	uchar *pfont = fontdata->video_fontdata +
			(u8)ch * fontdata->char_pixel_bytes;

	if (x_frac + VID_TO_POS(vc_priv->x_charsize) > vc_priv->xsize_frac)
		return -EAGAIN;
	linenum = y;
	x = VID_TO_PIXEL(x_frac);
	start = vid_priv->fb + linenum * vid_priv->line_length + x * pbytes;
	line = start;

	if (x_frac + VID_TO_POS(vc_priv->x_charsize) > vc_priv->xsize_frac)
		return -EAGAIN;

	ret = fill_char_vertically(pfont, &line, vid_priv, fontdata, NORMAL_DIRECTION);
	if (ret)
		return ret;

	ret = vidconsole_sync_copy(dev, start, line);
	if (ret)
		return ret;

	return VID_TO_POS(fontdata->width);
}

static int console_probe(struct udevice *dev)
{
	return console_set_font(dev, &fonts[0]);
}

struct vidconsole_ops console_ops = {
	.putc_xy	= console_putc_xy,
	.move_rows	= console_move_rows,
	.set_row	= console_set_row,
};

U_BOOT_DRIVER(vidconsole_normal) = {
	.name		= "vidconsole0",
	.id		= UCLASS_VIDEO_CONSOLE,
	.ops		= &console_ops,
	.probe		= console_probe,
	.priv_auto	= sizeof(struct console_simple_priv),
};

#if (CONFIG_IS_ENABLED(CONSOLE_ROTATION))
static int console_set_row_1(struct udevice *dev, uint row, int clr)
{
	struct video_priv *vid_priv = dev_get_uclass_priv(dev->parent);
	struct console_simple_priv *priv = dev_get_priv(dev);
	struct video_fontdata *fontdata = priv->fontdata;
	int pbytes = VNBYTES(vid_priv->bpix);
	void *start, *dst, *line;
	int i, j;
	int ret;

	start = vid_priv->fb + vid_priv->line_length -
		(row + 1) * fontdata->height * pbytes;
	line = start;
	for (j = 0; j < vid_priv->ysize; j++) {
		dst = line;
		for (i = 0; i < fontdata->height; i++)
			fill_pixel_and_goto_next(&dst, clr, pbytes, pbytes);
		line += vid_priv->line_length;
	}
	ret = vidconsole_sync_copy(dev, start, line);
	if (ret)
		return ret;

	return 0;
}

static int console_move_rows_1(struct udevice *dev, uint rowdst, uint rowsrc,
				   uint count)
{
	struct video_priv *vid_priv = dev_get_uclass_priv(dev->parent);
	struct console_simple_priv *priv = dev_get_priv(dev);
	struct video_fontdata *fontdata = priv->fontdata;
	int pbytes = VNBYTES(vid_priv->bpix);
	void *dst;
	void *src;
	int j, ret;

	dst = vid_priv->fb + vid_priv->line_length -
		(rowdst + count) * fontdata->height * pbytes;
	src = vid_priv->fb + vid_priv->line_length -
		(rowsrc + count) * fontdata->height * pbytes;

	for (j = 0; j < vid_priv->ysize; j++) {
		ret = vidconsole_memmove(dev, dst, src,
					fontdata->height * pbytes * count);
		if (ret)
			return ret;
		src += vid_priv->line_length;
		dst += vid_priv->line_length;
	}

	return 0;
}

static int console_putc_xy_1(struct udevice *dev, uint x_frac, uint y, char ch)
{
	struct vidconsole_priv *vc_priv = dev_get_uclass_priv(dev);
	struct udevice *vid = dev->parent;
	struct video_priv *vid_priv = dev_get_uclass_priv(vid);
	struct console_simple_priv *priv = dev_get_priv(dev);
	struct video_fontdata *fontdata = priv->fontdata;
	int pbytes = VNBYTES(vid_priv->bpix);
	int x, linenum, ret;
	void *start, *line;
	uchar *pfont = fontdata->video_fontdata +
			(u8)ch * fontdata->char_pixel_bytes;

	if (x_frac + VID_TO_POS(vc_priv->x_charsize) > vc_priv->xsize_frac)
		return -EAGAIN;
	linenum = VID_TO_PIXEL(x_frac) + 1;
	x = y + 1;
	start = vid_priv->fb + linenum * vid_priv->line_length - x * pbytes;
	line = start;

	ret = fill_char_horizontally(pfont, &line, vid_priv, fontdata, FLIPPED_DIRECTION);
	if (ret)
		return ret;

	/* We draw backwards from 'start, so account for the first line */
	ret = vidconsole_sync_copy(dev, start - vid_priv->line_length, line);
	if (ret)
		return ret;

	return VID_TO_POS(fontdata->width);
}


static int console_set_row_2(struct udevice *dev, uint row, int clr)
{
	struct video_priv *vid_priv = dev_get_uclass_priv(dev->parent);
	struct console_simple_priv *priv = dev_get_priv(dev);
	struct video_fontdata *fontdata = priv->fontdata;
	void *start, *line, *dst, *end;
	int pixels = fontdata->height * vid_priv->xsize;
	int i, ret;
	int pbytes = VNBYTES(vid_priv->bpix);

	start = vid_priv->fb + vid_priv->ysize * vid_priv->line_length -
		(row + 1) * fontdata->height * vid_priv->line_length;
	line = start;
	dst = line;
	for (i = 0; i < pixels; i++)
		fill_pixel_and_goto_next(&dst, clr, pbytes, pbytes);
	end = dst;
	ret = vidconsole_sync_copy(dev, start, end);
	if (ret)
		return ret;

	return 0;
}

static int console_move_rows_2(struct udevice *dev, uint rowdst, uint rowsrc,
			       uint count)
{
	struct video_priv *vid_priv = dev_get_uclass_priv(dev->parent);
	struct console_simple_priv *priv = dev_get_priv(dev);
	struct video_fontdata *fontdata = priv->fontdata;
	void *dst;
	void *src;
	void *end;

	end = vid_priv->fb + vid_priv->ysize * vid_priv->line_length;
	dst = end - (rowdst + count) * fontdata->height *
		vid_priv->line_length;
	src = end - (rowsrc + count) * fontdata->height *
		vid_priv->line_length;
	vidconsole_memmove(dev, dst, src,
			   fontdata->height * vid_priv->line_length * count);

	return 0;
}

static int console_putc_xy_2(struct udevice *dev, uint x_frac, uint y, char ch)
{
	struct vidconsole_priv *vc_priv = dev_get_uclass_priv(dev);
	struct udevice *vid = dev->parent;
	struct video_priv *vid_priv = dev_get_uclass_priv(vid);
	struct console_simple_priv *priv = dev_get_priv(dev);
	struct video_fontdata *fontdata = priv->fontdata;
	int pbytes = VNBYTES(vid_priv->bpix);
	int linenum, x, ret;
	void *start, *line;
	uchar *pfont = fontdata->video_fontdata +
			(u8)ch * fontdata->char_pixel_bytes;

	if (x_frac + VID_TO_POS(vc_priv->x_charsize) > vc_priv->xsize_frac)
		return -EAGAIN;
	linenum = vid_priv->ysize - y - 1;
	x = vid_priv->xsize - VID_TO_PIXEL(x_frac) - 1;
	start = vid_priv->fb + linenum * vid_priv->line_length + x * pbytes;
	line = start;

	ret = fill_char_vertically(pfont, &line, vid_priv, fontdata, FLIPPED_DIRECTION);
	if (ret)
		return ret;

	/* Add 4 bytes to allow for the first pixel writen */
	ret = vidconsole_sync_copy(dev, start + 4, line);
	if (ret)
		return ret;

	return VID_TO_POS(fontdata->width);
}

static int console_set_row_3(struct udevice *dev, uint row, int clr)
{
	struct video_priv *vid_priv = dev_get_uclass_priv(dev->parent);
	struct console_simple_priv *priv = dev_get_priv(dev);
	struct video_fontdata *fontdata = priv->fontdata;
	int pbytes = VNBYTES(vid_priv->bpix);
	void *start, *dst, *line;
	int i, j, ret;

	start = vid_priv->fb + row * fontdata->height * pbytes;
	line = start;
	for (j = 0; j < vid_priv->ysize; j++) {
		dst = line;
		for (i = 0; i < fontdata->height; i++)
			fill_pixel_and_goto_next(&dst, clr, pbytes, pbytes);
		line += vid_priv->line_length;
	}
	ret = vidconsole_sync_copy(dev, start, line);
	if (ret)
		return ret;

	return 0;
}

static int console_move_rows_3(struct udevice *dev, uint rowdst, uint rowsrc,
			       uint count)
{
	struct video_priv *vid_priv = dev_get_uclass_priv(dev->parent);
	struct console_simple_priv *priv = dev_get_priv(dev);
	struct video_fontdata *fontdata = priv->fontdata;
	int pbytes = VNBYTES(vid_priv->bpix);
	void *dst;
	void *src;
	int j, ret;

	dst = vid_priv->fb + rowdst * fontdata->height * pbytes;
	src = vid_priv->fb + rowsrc * fontdata->height * pbytes;

	for (j = 0; j < vid_priv->ysize; j++) {
		ret = vidconsole_memmove(dev, dst, src,
					fontdata->height * pbytes * count);
		if (ret)
			return ret;
		src += vid_priv->line_length;
		dst += vid_priv->line_length;
	}

	return 0;
}

static int console_putc_xy_3(struct udevice *dev, uint x_frac, uint y, char ch)
{
	struct vidconsole_priv *vc_priv = dev_get_uclass_priv(dev);
	struct udevice *vid = dev->parent;
	struct video_priv *vid_priv = dev_get_uclass_priv(vid);
	struct console_simple_priv *priv = dev_get_priv(dev);
	struct video_fontdata *fontdata = priv->fontdata;
	int pbytes = VNBYTES(vid_priv->bpix);
	int linenum, x, ret;
	void *start, *line;
	uchar *pfont = fontdata->video_fontdata +
			(u8)ch * fontdata->char_pixel_bytes;

	if (x_frac + VID_TO_POS(vc_priv->x_charsize) > vc_priv->xsize_frac)
		return -EAGAIN;
	x = y;
	linenum = vid_priv->ysize - VID_TO_PIXEL(x_frac) - 1;
	start = vid_priv->fb + linenum * vid_priv->line_length + y * pbytes;
	line = start;

	ret = fill_char_horizontally(pfont, &line, vid_priv, fontdata, NORMAL_DIRECTION);
	if (ret)
		return ret;
	/* Add a line to allow for the first pixels writen */
	ret = vidconsole_sync_copy(dev, start + vid_priv->line_length, line);
	if (ret)
		return ret;

	return VID_TO_POS(fontdata->width);
}

struct vidconsole_ops console_ops_1 = {
	.putc_xy	= console_putc_xy_1,
	.move_rows	= console_move_rows_1,
	.set_row	= console_set_row_1,
};

struct vidconsole_ops console_ops_2 = {
	.putc_xy	= console_putc_xy_2,
	.move_rows	= console_move_rows_2,
	.set_row	= console_set_row_2,
};

struct vidconsole_ops console_ops_3 = {
	.putc_xy	= console_putc_xy_3,
	.move_rows	= console_move_rows_3,
	.set_row	= console_set_row_3,
};

U_BOOT_DRIVER(vidconsole_1) = {
	.name		= "vidconsole1",
	.id		= UCLASS_VIDEO_CONSOLE,
	.ops		= &console_ops_1,
	.probe		= console_probe,
	.priv_auto	= sizeof(struct console_simple_priv),
};

U_BOOT_DRIVER(vidconsole_2) = {
	.name		= "vidconsole2",
	.id		= UCLASS_VIDEO_CONSOLE,
	.ops		= &console_ops_2,
	.probe		= console_probe,
	.priv_auto	= sizeof(struct console_simple_priv),
};

U_BOOT_DRIVER(vidconsole_3) = {
	.name		= "vidconsole3",
	.id		= UCLASS_VIDEO_CONSOLE,
	.ops		= &console_ops_3,
	.probe		= console_probe,
	.priv_auto	= sizeof(struct console_simple_priv),
};
#endif
