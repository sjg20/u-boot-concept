/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Internal header file for bootflow
 *
 * Copyright 2022 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __BOOTFLOW_INTERNAL_H
#define __BOOTFLOW_INTERNAL_H

/* expo IDs for elements of the bootflow menu */
enum {
	START,

	/* scene */
	MAIN,

	/* objects */
	OBJ_U_BOOT_LOGO,
	OBJ_MENU,
	OBJ_PROMPT,
	OBJ_MENU_TITLE,
	OBJ_POINTER,

	/* menu items / components (bootflow number is added to these) */
	ITEM = 100,
	ITEM_LABEL = 200,
	ITEM_DESC = 300,
	ITEM_KEY = 400,
	ITEM_PREVIEW = 500,

	/* left margin for the main menu */
	MARGIN_LEFT	 = 100,
};

#endif /* __BOOTFLOW_INTERNAL_H */
