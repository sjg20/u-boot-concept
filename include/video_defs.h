/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Video definitions
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#ifndef __VIDEO_DEFS_H
#define __VIDEO_DEFS_H

/* Maximum length of an embedded image name */
#define VIDEO_IMAGE_NAMELEN	16

#ifndef __ASSEMBLY__

#include <stdbool.h>

/**
 * struct vid_bbox - Represents a bounding box for video operations
 *
 * The bounding box is only valid if x1 > x0 and y1 > y0. An invalid bounding
 * box (where x1 <= x0 or y1 <= y0) indicates that there is no area to process.
 *
 * @x0: X start position in pixels from the left
 * @y0: Y start position in pixels from the top
 * @x1: X end position in pixels from the left
 * @y1: Y end position in pixels from the top
 */
struct vid_bbox {
	int x0;
	int y0;
	int x1;
	int y1;
};

/**
 * vid_bbox_valid() - Check if a bounding box is valid
 *
 * A valid bounding box has x1 > x0 and y1 > y0. An invalid/inverted bounding
 * box (where x1 <= x0 or y1 <= y0) indicates that there is no area to process.
 *
 * @bbox: Bounding box to check
 * Return: true if valid, false if invalid/inverted
 */
static inline bool vid_bbox_valid(const struct vid_bbox *bbox)
{
	return bbox->x1 > bbox->x0 && bbox->y1 > bbox->y0;
}

/**
 * struct vid_pos - Represents a position for video operations
 *
 * @x: X coordinate in pixels from the left
 * @y: Y coordinate in pixels from the top
 */
struct vid_pos {
	int x;
	int y;
};

#endif /* __ASSEMBLY__ */

#endif /* __VIDEO_DEFS_H */
