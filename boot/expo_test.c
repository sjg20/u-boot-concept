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
}

void expo_test_update(struct expo *exp)
{
	struct expo_test_mode *test = exp->test;

	test->render_count++;
}

int expo_test_render(struct expo *exp)
{
	struct expo_test_mode *test = exp->test;
	struct vidconsole_priv *cons_priv;
	struct udevice *dev = exp->display;
	struct video_priv *vid_priv;
	char buf[30];
	int x, y;
	int ret;

	if (!test->enabled)
		return 0;

	/* Select 8x16 font for test display */
	ret = vidconsole_select_font(exp->cons, "8x16", 0);
	if (ret && ret != -ENOSYS)
		return log_msg_ret("font", ret);

	vid_priv = dev_get_uclass_priv(dev);
	cons_priv = dev_get_uclass_priv(exp->cons);

	/* Display frame count */
	snprintf(buf, sizeof(buf), "frame  %6d", test->render_count);
	x = vid_priv->xsize - 18 * cons_priv->x_charsize;
	y = 10;
	vidconsole_set_cursor_pos(exp->cons, x, y);
	vidconsole_put_string(exp->cons, buf);

	return 0;
}
