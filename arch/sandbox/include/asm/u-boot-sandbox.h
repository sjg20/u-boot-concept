/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2011 The Chromium OS Authors.
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Alex Zuepke <azu@sysgo.de>
 */

#ifndef _U_BOOT_SANDBOX_H_
#define _U_BOOT_SANDBOX_H_

#include <linux/compiler_attributes.h>

/* board/.../... */
int board_init(void);

/* start.c */
int sandbox_early_getopt_check(void);
int sandbox_main_loop_init(void);

/* drivers/video/sandbox_sdl.c */
int sandbox_lcd_sdl_early_init(void);

struct udevice;

/**
 * sandbox_reset() - reset sandbox
 *
 * This functions implements the cold reboot of the sandbox. It relaunches the
 * U-Boot binary with the same command line parameters as the original call.
 * The PID of the process stays the same. All file descriptors that have not
 * been opened with O_CLOEXEC stay open including stdin, stdout, stderr.
 */
void sandbox_reset(void);

/* Exit sandbox (quit U-Boot) */
void __noreturn sandbox_exit(void);

#endif	/* _U_BOOT_SANDBOX_H_ */
