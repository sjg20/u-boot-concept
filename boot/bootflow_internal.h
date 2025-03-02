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

	/* strings */
	STR_PROMPT1A,
	STR_PROMPT1B,
	STR_PROMPT2,
	STR_PROMPT3,
	STR_AUTOBOOT,
	STR_MENU_TITLE,
	STR_POINTER,
	STR_TEXTEDIT,

	/* scene */
	MAIN,

	/* objects */
	OBJ_U_BOOT_LOGO,
	OBJ_BOX,
	OBJ_MENU,
	OBJ_PROMPT1A,
	OBJ_PROMPT1B,
	OBJ_PROMPT2,
	OBJ_PROMPT3,
	OBJ_MENU_TITLE,
	OBJ_POINTER,
	OBJ_AUTOBOOT,
	OBJ_TEXTEDIT,
	// OBJ_TEXTEDIT_LINES,
	// OBJ_TEXTEDIT_END = OBJ_TEXTEDIT_LINES + 50,

	/* strings for menu items */
	STR_LABEL = 100,
	STR_DESC = 200,
	STR_KEY = 300,

	/* menu items / components (bootflow number is added to these) */
	ITEM = 500,
	ITEM_LABEL = 600,
	ITEM_DESC = 700,
	ITEM_KEY = 800,
	ITEM_PREVIEW = 900,

	/* left margin for the main menu */
	MARGIN_LEFT	 = 100,
};

#endif /* __BOOTFLOW_INTERNAL_H */
