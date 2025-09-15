// SPDX-License-Identifier: GPL-2.0+
/*
 * Common functions for expo tests
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <expo.h>
#include <test/ut.h>
#include "../../boot/scene_internal.h"

int click_check(struct unit_test_state *uts, struct scene *scn, uint id,
		enum expoact_type expect_type, struct expo_action *act)
{
	struct scene_obj *obj;

	obj = scene_obj_find(scn, id, SCENEOBJT_NONE);
	ut_assertnonnull(obj);
	ut_assertok(scene_send_click(scn, (obj->bbox.x0 + obj->bbox.x1) / 2,
				     obj->bbox.y0 + 5, act));
	ut_asserteq(expect_type, act->type);

	return 0;
}
