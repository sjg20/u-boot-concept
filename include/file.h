// SPDX-License-Identifier: GPL-2.0+
/*
 * Implementation of a file containing data
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#ifndef __file_h
#define __file_h

#include <linux/types.h>

/**
 * struct file - Information about an open file
 *
 * @pos: Current file position
 */
struct file {
	loff_t pos;
};

int file_open(struct udevice *fs,

#endif
