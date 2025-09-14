// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the driver model mouse API
 *
 * Copyright 2025 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <dm.h>
#include <mouse.h>
#include <dm/test.h>
#include <test/test.h>
#include <test/ut.h>
#include <asm/test.h>

static int dm_test_mouse_basic(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_assertok(uclass_first_device_err(UCLASS_MOUSE, &dev));

	/* put mouse in test mode */
	sandbox_mouse_set_test_mode(dev, true);

	return 0;
}
DM_TEST(dm_test_mouse_basic, UTF_SCAN_PDATA | UTF_SCAN_FDT);

static int dm_test_mouse_motion(struct unit_test_state *uts)
{
	struct udevice *dev;
	struct mouse_event event, inject;

	ut_assertok(uclass_first_device_err(UCLASS_MOUSE, &dev));

	/* put mouse in test mode */
	sandbox_mouse_set_test_mode(dev, true);

	/* inject a motion event */
	inject.type = MOUSE_EV_MOTION;
	inject.motion.state = 0;
	inject.motion.x = 100;
	inject.motion.y = 200;
	inject.motion.xrel = 10;
	inject.motion.yrel = 20;

	sandbox_mouse_inject(dev, &inject);

	/* get and verify the event */
	ut_assertok(mouse_get_event(dev, &event));
	ut_asserteq(MOUSE_EV_MOTION, event.type);
	ut_asserteq(0, event.motion.state);
	ut_asserteq(100, event.motion.x);
	ut_asserteq(200, event.motion.y);
	ut_asserteq(10, event.motion.xrel);
	ut_asserteq(20, event.motion.yrel);

	/* verify no more events are pending */
	ut_asserteq(-EAGAIN, mouse_get_event(dev, &event));

	return 0;
}
DM_TEST(dm_test_mouse_motion, UTF_SCAN_PDATA | UTF_SCAN_FDT);

static int dm_test_mouse_button(struct unit_test_state *uts)
{
	struct udevice *dev;
	struct mouse_event event, inject;

	ut_assertok(uclass_first_device_err(UCLASS_MOUSE, &dev));

	/* put mouse in test mode */
	sandbox_mouse_set_test_mode(dev, true);

	/* inject a button press event */
	inject.type = MOUSE_EV_BUTTON;
	inject.button.button = BUTTON_LEFT;
	inject.button.press_state = BUTTON_PRESSED;
	inject.button.clicks = 1;
	inject.button.x = 150;
	inject.button.y = 250;

	sandbox_mouse_inject(dev, &inject);

	/* get and verify the event */
	ut_assertok(mouse_get_event(dev, &event));
	ut_asserteq(MOUSE_EV_BUTTON, event.type);
	ut_asserteq(BUTTON_LEFT, event.button.button);
	ut_asserteq(BUTTON_PRESSED, event.button.press_state);
	ut_asserteq(1, event.button.clicks);
	ut_asserteq(150, event.button.x);
	ut_asserteq(250, event.button.y);

	return 0;
}
DM_TEST(dm_test_mouse_button, UTF_SCAN_PDATA | UTF_SCAN_FDT);
