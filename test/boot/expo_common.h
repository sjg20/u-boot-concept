/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Common functions for expo tests
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#ifndef __expo_common_h
#define __expo_common_h

#include <linux/types.h>

enum expoact_type;
struct expo_action;
struct scene;
struct unit_test_state;

/**
 * click_check() - Click on an object and check the resulting action type
 *
 * Clicks halfway along the object, 5 pixels from the top
 *
 * @uts: Test state
 * @scn: Scene containing the object
 * @id: ID of object to click on
 * @expect_type: Expected action type
 * @act: Returns the action from the click
 * Returns: 0 if OK, 1 if assertion failed
 */
int click_check(struct unit_test_state *uts, struct scene *scn, uint id,
		enum expoact_type expect_type, struct expo_action *act);

#endif
