/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Internal header file for bootflow
 *
 * Copyright 2022 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __BOOTFLOW_INTERNAL_H
#define __BOOTFLOW_INTERNAL_H

/**
 * enum boomenu_id_t - expo IDs for elements of the bootflow menu
 *
 * @OBJ_OTHER_LOGO: Second logo (separate from the U-Boot logo)
 *
 * The ranges below are as follows:
 *
 * @ITEM: Menu items
 * @ITEM_LABEL: Short Media or other label to indicate what it is, e.g. "mmc0"
 * @ITEM_DESC: Longer description or pretty name, e.g. "Ubuntu 2024.04 LTS"
 * @ITEM_KEY: Keypress to select this item, e.g. "1"
 * @ITEM_PREVIEW: Preview image for the OS
 */
enum boomenu_id_t {
	START,

	/* strings */
	STR_PROMPT1A,
	STR_PROMPT1B,
	STR_PROMPT2,
	STR_AUTOBOOT,
	STR_MENU_TITLE,
	STR_POINTER,

	/* scene */
	MAIN,

	/* objects */
	OBJ_U_BOOT_LOGO,
	OBJ_BOX,
	OBJ_MENU,
	OBJ_PROMPT1A,
	OBJ_PROMPT1B,
	OBJ_PROMPT2,
	OBJ_MENU_TITLE,
	OBJ_POINTER,
	OBJ_AUTOBOOT,
	OBJ_OTHER_LOGO,

	/* strings for menu items */
	STR_LABEL = 100,
	STR_DESC = 200,
	STR_KEY = 300,

	/* menu items / components (bootflow number is added to these) */
	ITEM = 400,
	ITEM_LABEL = 500,
	ITEM_DESC = 600,
	ITEM_KEY = 700,
	ITEM_PREVIEW = 800,

	/* left margin for the main menu */
	MARGIN_LEFT	 = 100,
};

#endif /* __BOOTFLOW_INTERNAL_H */
