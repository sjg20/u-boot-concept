// SPDX-License-Identifier: GPL-2.0+
/*
 * Expo test mode
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY	LOGC_EXPO

#include <env.h>
#include <errno.h>
#include <expo.h>
#include <expo_test.h>
#include <log.h>
#include <malloc.h>

int expo_test_init(struct expo *exp)
{
	struct expo_test_mode *test;

	test = calloc(1, sizeof(struct expo_test_mode));
	if (!test)
		return log_msg_ret("test", -ENOMEM);

	test->enabled = env_get_yesno("expotest") == 1;
	exp->test = test;

	return 0;
}

void expo_test_uninit(struct expo *exp)
{
	free(exp->test);
	exp->test = NULL;
}

void expo_test_update(struct expo *exp)
{
	struct expo_test_mode *test = exp->test;

	if (!test)
		return;

	test->render_count++;
}
