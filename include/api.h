/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * U-Boot Library API
 *
 * This provides the API for programs that use the U-Boot library (libu-boot.so
 * or libu-boot.a). It includes functions that are renamed to avoid conflicts
 * with standard C library functions.
 *
 * Copyright 2025 Canonical
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#ifndef __API_H
#define __API_H

/**
 * ub_printf() - U-Boot printf function
 *
 * This is U-Boot's printf implementation, which uses the U-Boot console system.
 * It only works after ulib_init() has been called to initialize the U-Boot
 * library.
 *
 * @fmt: Format string
 * @...: Arguments for the format string
 * Return: number of characters written
 */
int ub_printf(const char *fmt, ...);

#endif /* __API_H */
