/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * U-Boot library interface
 *
 * This provides basic access to setup of the U-Boot library.
 *
 * Library functions must be individually accessed via their respective headers.
 *
 * Copyright 2025 Canonical
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#ifndef __U_BOOT_LIB_H
#define __U_BOOT_LIB_H

struct global_data;

/**
 * ulib_init() - set up the U-Boot library
 *
 * @progname: Program name to use (must be a writeable string, typically argv[0])
 * @data: Global data (must remain valid until the program exits)
 * Return: 0 if OK, -ve error code on error
 */
int ulib_init(char *progname);

/**
 * ulib_uninit() shut down the U-Boot librrary
 *
 * Call this when your program has finished using the library, before it exits
 */
void ulib_uninit(void);

#endif
