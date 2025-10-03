// SPDX-License-Identifier: GPL-2.0+
/*
 * Expo test mode
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY	LOGC_EXPO

#include <expo.h>
#include <expo_test.h>

int expo_test_init(struct expo *exp)
{
	return 0;
}

void expo_test_uninit(struct expo *exp)
{
}

void expo_test_update(struct expo *exp)
{
}
