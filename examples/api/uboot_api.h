/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * U-Boot Library API for External Programs
 *
 * This header provides declarations for U-Boot library functions that can be
 * used by external programs without conflicts with system headers.
 *
 * Copyright 2025 Canonical
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#ifndef UBOOT_API_H
#define UBOOT_API_H

#include <sys/types.h>

/* Library initialization */
int ulib_init(char *progname);
void ulib_uninit(void);

/* OS functions from U-Boot sandbox */
int os_open(const char *pathname, int flags);
int os_close(int fd);
ssize_t os_read(int fd, void *buf, size_t count);
char *os_fgets(char *str, int size, int fd);

#endif /* UBOOT_API_H */