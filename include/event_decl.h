/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Declarations needed by events
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#ifndef __event_decl_h
#define __event_decl_h

#include <linux/bitops.h>

/**
 * enum bootm_final_t - flags to control bootm_final()
 *
 * Note that this is defined in event.h since it is used by events
 *
 * @BOOTM_FINAL_FAKE: true to do everything except actually boot; it then
 *	returns to the caller
 * @BOOTM_FINAL_NO_CLEANUP: true to skip calling cleanup_before_linux()
 */
enum bootm_final_t {
	BOOTM_FINAL_FAKE	= BIT(0),
	BOOTM_FINAL_NO_CLEANUP	= BIT(1),
};

#endif
