/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Utility functions
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#ifndef __bootctl_util_h
#define __bootctl_util_h

struct alist;
struct udevice;

/**
 * bc_printf() - Print a string to the display
 *
 * @fmt: printf() format string for log record
 * @...: Optional parameters, according to the format string @fmt
 * Return: Number of characters emitted
 */
int bc_printf(struct udevice *disp, const char *fmt, ...)
		__attribute__ ((format (__printf__, 2, 3)));

/**
 * show_measures() - Show measurements that have been completed
 *
 * @result: List of struct measure_info records
 */
void show_measures(struct alist *result);

#endif
