/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright Canonical Ltd
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __EXPO_TEST_H
#define __EXPO_TEST_H

struct expo;

/* Number of seconds to average FPS over in test mode */
#define EXPO_FPS_AVG_SECONDS	5

/**
 * struct expo_test_mode - Test mode information for expo
 *
 * @enabled: true if test mode is enabled
 * @start_time_ms: Time when expo_enter_mode() was called (milliseconds)
 * @render_count: Number of calls to expo_render() since expo_enter_mode()
 * @fps_timestamps_ms: Timestamps for FPS calculation (milliseconds)
 * @fps_frame_counts: Frame counts at each timestamp
 * @fps_index: Current index in the FPS tracking arrays
 * @fps_last: Last calculated FPS value
 * @last_update: Time of last FPS update (milliseconds)
 */
struct expo_test_mode {
	bool enabled;
	ulong start_time_ms;
	int render_count;
	ulong fps_timestamps_ms[EXPO_FPS_AVG_SECONDS];
	int fps_frame_counts[EXPO_FPS_AVG_SECONDS];
	int fps_index;
	int fps_last;
	ulong last_update;
};

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
 * expo_test_checkenv() - Check environment and reset test mode
 *
 * @exp: Expo to update test mode for
 *
 * Checks the expotest environment variable and updates the enabled flag
 * accordingly. Also resets the render count to 0.
 */
void expo_test_checkenv(struct expo *exp);

/**
 * expo_test_update() - Update test mode counters
 *
 * @exp: Expo to update test mode for
 */
void expo_test_update(struct expo *exp);

/**
 * expo_test_render() - Render test mode information
 *
 * @exp: Expo to render test info for
 * Return: 0 if OK, -ve on error
 */
int expo_test_render(struct expo *exp);

/**
 * expo_calc_fps() - Calculate FPS based on recent frame history
 *
 * @test: Test mode data containing frame history
 * Return: Calculated FPS value, or 0 if insufficient data
 */
int expo_calc_fps(struct expo_test_mode *test);

#else

static inline int expo_test_init(struct expo *exp)
{
	return 0;
}

static inline void expo_test_uninit(struct expo *exp)
{
}

static inline void expo_test_checkenv(struct expo *exp)
{
}

static inline void expo_test_update(struct expo *exp)
{
}

static inline int expo_test_render(struct expo *exp)
{
	return 0;
}

static inline int expo_calc_fps(struct expo_test_mode *test)
{
	return 0;
}

#endif /* EXPO_TEST */

#endif /* __EXPO_TEST_H */
