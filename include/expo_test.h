/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright Canonical Ltd
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __EXPO_TEST_H
#define __EXPO_TEST_H

struct expo;

#if CONFIG_IS_ENABLED(EXPO_TEST)

/**
 * expo_test_init() - Initialize test mode for an expo
 *
 * @exp: Expo to initialize test mode for
 * Return: 0 if OK, -ve on error
 */
int expo_test_init(struct expo *exp);

/**
 * expo_test_uninit() - Uninitialize test mode for an expo
 *
 * @exp: Expo to uninitialize test mode for
 */
void expo_test_uninit(struct expo *exp);

/**
 * expo_test_update() - Update test mode counters
 *
 * @exp: Expo to update test mode for
 */
void expo_test_update(struct expo *exp);

#else

static inline int expo_test_init(struct expo *exp)
{
	return 0;
}

static inline void expo_test_uninit(struct expo *exp)
{
}

static inline void expo_test_update(struct expo *exp)
{
}

#endif /* EXPO_TEST */

#endif /* __EXPO_TEST_H */
