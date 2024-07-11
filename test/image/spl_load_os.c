// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <image.h>
#include <os.h>
#include <spl.h>
#include <test/spl.h>
#include <test/ut.h>

static int spl_test_load(struct unit_test_state *uts)
{
	struct spl_image_info image;

	ut_assertok(sandbox_spl_load_fit(&image));

	return 0;
}
SPL_TEST(spl_test_load, 0);
