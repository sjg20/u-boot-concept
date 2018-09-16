/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Taken from coreboot file bitmap.h
 *
 * Copyright 2015 Google Inc.
 */

#ifndef __CB_BITMAP_H__
#define __CB_BITMAP_H__

struct bitmap_file_header {
	uint8_t signature[2];
	uint32_t file_size;
	uint16_t reserved[2];
	uint32_t bitmap_offset;
} __packed;

/* Bitmap version 3 */

struct bitmap_header_v3 {
	uint32_t header_size;
	int32_t width;
	int32_t height;
	uint16_t planes;
	uint16_t bits_per_pixel;
	uint32_t compression;
	uint32_t size;
	int32_t h_res;
	int32_t v_res;
	uint32_t colors_used;
	uint32_t colors_important;
} __packed;

struct bitmap_palette_element_v3 {
	uint8_t blue;
	uint8_t green;
	uint8_t red;
	uint8_t reserved;
} __packed;

#endif /* __CB_BITMAP_H__ */
