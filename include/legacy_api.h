/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2017 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __LEGACY_API_H
#define __LEGACY_API_H

/**
 * legacy_api_init() - Initialize legacy API for external applications
 *
 * Initialize API for external (standalone) applications running on top of
 * U-Boot. It is called during the generic post-relocation init sequence.
 *
 * Note that this is deprecated.
 *
 * Return: 0 if OK
 */
int legacy_api_init(void);

#endif
