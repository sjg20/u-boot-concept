// SPDX-License-Identifier: GPL-2.0+
/*
 * Expo test mode
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY	LOGC_EXPO

#include <dm.h>
#include <env.h>
#include <errno.h>
#include <expo.h>
#include <expo_test.h>
#include <log.h>
#include <malloc.h>
#include <time.h>
#include <video.h>
#include <video_console.h>

int expo_test_init(struct expo *exp)
{
	struct expo_test_mode *test;

	test = calloc(1, sizeof(struct expo_test_mode));
	if (!test)
		return log_msg_ret("test", -ENOMEM);

	exp->test = test;
	expo_test_checkenv(exp);

	return 0;
}

void expo_test_uninit(struct expo *exp)
{
	free(exp->test);
	exp->test = NULL;
}

void expo_test_checkenv(struct expo *exp)
{
	struct expo_test_mode *test = exp->test;

	test->enabled = env_get_yesno("expotest") == 1;
	test->render_count = 0;
	test->start_time_ms = get_timer(0);
	test->last_update = get_timer(0);
}

void expo_test_mark(struct expo *exp)
{
	struct expo_test_mode *test = exp->test;

	test->base_time_us = timer_get_us();
}

void expo_test_update(struct expo *exp)
{
	struct expo_test_mode *test = exp->test;

	test->render_count++;
}

void expo_test_sync(struct expo *exp)
{
	struct expo_test_mode *test = exp->test;

	test->sync_delta_us = get_timer_us(test->base_time_us);
}

void expo_test_poll(struct expo *exp)
{
	struct expo_test_mode *test = exp->test;

	test->poll_delta_us = get_timer_us(test->base_time_us);
}

int expo_calc_fps(struct expo_test_mode *test)
{
	ulong oldest_time, newest_time;
	int oldest_frames, newest_frames;
	int frame_delta, time_delta;
	int oldest_idx;
	int fps;

	/* Use most recent entry */
	newest_time = test->fps_timestamps_ms[test->fps_index];
	newest_frames = test->fps_frame_counts[test->fps_index];

	/* Find oldest valid entry by looking backwards from current index */
	oldest_idx = (test->fps_index + 1) % EXPO_FPS_AVG_SECONDS;
	if (test->fps_timestamps_ms[oldest_idx] == 0) {
		/* Array hasn't wrapped yet, use first entry */
		oldest_idx = 0;
	}

	oldest_time = test->fps_timestamps_ms[oldest_idx];
	oldest_frames = test->fps_frame_counts[oldest_idx];

	/* Need at least two data points with different timestamps */
	if (oldest_time >= newest_time)
		return 0;

	frame_delta = newest_frames - oldest_frames;
	time_delta = newest_time - oldest_time;

	if (!time_delta)
		return 0;

	/* Calculate FPS: frames / (time_ms / 1000) */
	fps = (frame_delta * 1000) / time_delta;

	return fps;
}

int expo_test_render(struct expo *exp)
{
	struct expo_test_mode *test = exp->test;
	struct vidconsole_priv *cons_priv;
	struct udevice *dev = exp->display;
	struct video_priv *vid_priv;
	char buf[30];
	ulong now;
	int x, y;
	int ret;

	if (!test->enabled)
		return 0;

	/* Calculate time between update and render */
	if (test->base_time_us)
		test->render_delta_us = get_timer_us(test->base_time_us);

	/* Select 8x16 font for test display */
	ret = vidconsole_select_font(exp->cons, "8x16", 0);
	if (ret && ret != -ENOSYS)
		return log_msg_ret("font", ret);

	vid_priv = dev_get_uclass_priv(dev);
	cons_priv = dev_get_uclass_priv(exp->cons);

	/* Accumulate delta times for averaging */
	test->render_total_us += test->render_delta_us;
	test->sync_total_us += test->sync_delta_us;
	test->poll_total_us += test->poll_delta_us;
	test->frame_count_last_sec++;

	/* Update FPS and averages if at least 1 second has elapsed */
	if (get_timer(test->last_update) >= 1000) {
		now = get_timer(test->start_time_ms);
		test->fps_index = (test->fps_index + 1) % EXPO_FPS_AVG_SECONDS;
		test->fps_timestamps_ms[test->fps_index] = now;
		test->fps_frame_counts[test->fps_index] = test->render_count;
		test->fps_last = expo_calc_fps(test);

		/* Calculate averages over the last second */
		if (test->frame_count_last_sec > 0) {
			test->render_avg_us = test->render_total_us /
				test->frame_count_last_sec;
			test->sync_avg_us = test->sync_total_us /
				test->frame_count_last_sec;
			test->poll_avg_us = test->poll_total_us /
				test->frame_count_last_sec;
		}

		/* Reset accumulation counters */
		test->render_total_us = 0;
		test->sync_total_us = 0;
		test->poll_total_us = 0;
		test->frame_count_last_sec = 0;

		test->last_update = get_timer(0);
	}

	/* Display frame count */
	snprintf(buf, sizeof(buf), "frame  %6d", test->render_count);
	x = vid_priv->xsize - 18 * cons_priv->x_charsize;
	y = 10;
	vidconsole_set_cursor_pos(exp->cons, x, y);
	vidconsole_put_string(exp->cons, buf);

	/* Display FPS on next line (only if non-zero) */
	if (test->fps_last > 0) {
		snprintf(buf, sizeof(buf), "fps    %6d", test->fps_last);
		y += cons_priv->y_charsize;
		vidconsole_set_cursor_pos(exp->cons, x, y);
		vidconsole_put_string(exp->cons, buf);
	}

	/* Display average render time in milliseconds on next line */
	snprintf(buf, sizeof(buf), "render %6lu.%01lums",
		 test->render_avg_us / 1000,
		 (test->render_avg_us % 1000) / 100);
	y += cons_priv->y_charsize;
	vidconsole_set_cursor_pos(exp->cons, x, y);
	vidconsole_put_string(exp->cons, buf);

	/* Display average sync time in milliseconds on next line */
	snprintf(buf, sizeof(buf), "sync   %6lu.%01lums",
		 test->sync_avg_us / 1000,
		 (test->sync_avg_us % 1000) / 100);
	y += cons_priv->y_charsize;
	vidconsole_set_cursor_pos(exp->cons, x, y);
	vidconsole_put_string(exp->cons, buf);

	/* Display average poll time in milliseconds on next line */
	snprintf(buf, sizeof(buf), "poll   %6lu.%01lums",
		 test->poll_avg_us / 1000,
		 (test->poll_avg_us % 1000) / 100);
	y += cons_priv->y_charsize;
	vidconsole_set_cursor_pos(exp->cons, x, y);
	vidconsole_put_string(exp->cons, buf);

	return 0;
}
