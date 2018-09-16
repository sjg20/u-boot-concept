/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Taken from depthcharge file of the same name
 *
 * Copyright 2015 Google Inc.
 */

#ifndef __CROS_SCREENS_H
#define __CROS_SCREENS_H

int vboot_draw_screen(uint32_t screen, uint32_t locale);
int vboot_draw_ui(uint32_t screen, uint32_t locale,
		  uint32_t selected_index, uint32_t disabled_idx_mask,
		  uint32_t redraw_base);

/**
 * Return number of supported locales
 *
 * @return number of supported locales
 */
int vboot_get_locale_count(void);

#endif /* __CROS_SCREENS_H */
