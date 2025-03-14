/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Common definitions for bootctl tests
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#ifndef __bootctl_common_h
#define __bootctl_common_h

#define BOOTCTL_TEST(_name, _flags)	UNIT_TEST(_name, _flags, bootctl)

#endif
