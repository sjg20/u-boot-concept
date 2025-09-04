/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Simplified U-Boot library interface
 *
 * This header provides a simplified API for using the U-Boot library
 * in external programs without needing to understand U-Boot internals.
 *
 * Copyright 2025 Canonical
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#ifndef __ULIB_H
#define __ULIB_H

/* Don't include U-Boot's stdio.h, use system headers */
#ifndef USE_HOSTCC
#define USE_HOSTCC
#endif

/* Forward declarations to avoid including complex headers */
struct global_data;

/**
 * ulib_simple_init() - Initialize U-Boot library with defaults
 *
 * This is a simplified initialization function that handles all the
 * complexity of setting up global_data and other U-Boot internals.
 *
 * @progname: Program name (typically argv[0])
 * Return: 0 on success, -1 on error
 */
int ulib_simple_init(const char *progname);

/**
 * ulib_cleanup() - Clean up U-Boot library resources
 *
 * Call this before exiting your program to clean up any resources
 * allocated by the U-Boot library.
 */
void ulib_cleanup(void);

/**
 * ulib_get_version() - Get U-Boot library version string
 *
 * Return: Version string
 */
const char *ulib_get_version(void);

/* Include commonly used U-Boot functions */
#ifdef ULIB_FULL_API
#include <os.h>
#endif

#endif /* __ULIB_H */