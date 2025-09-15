// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <dm.h>
#include <errno.h>
#include <mouse.h>

int mouse_get_event(struct udevice *dev, struct mouse_event *evt)
{
	struct mouse_ops *ops = mouse_get_ops(dev);
	int ret;

	if (!ops->get_event)
		return -ENOSYS;

	ret = ops->get_event(dev, evt);
	if (ret)
		return ret;

	return 0;
}

int mouse_get_click(struct udevice *dev, int *xp, int *yp)
{
	struct mouse_uc_priv *uc_priv = dev_get_uclass_priv(dev);
	struct mouse_event event;
	int ret;

	/* Get one mouse event */
	ret = mouse_get_event(dev, &event);
	if (ret)
		return -EAGAIN; /* No event available */

	/* Only process button events for left button */
	if (event.type == MOUSE_EV_BUTTON &&
	    event.button.button == BUTTON_LEFT) {
		enum mouse_press_state_t new_state = event.button.press_state;
		bool pending = false;

		/* Detect press->release transition (click) */
		if (uc_priv->left_button_state == BUTTON_PRESSED &&
		    new_state == BUTTON_RELEASED) {
			pending = true;
			uc_priv->click_x = event.button.x;
			uc_priv->click_y = event.button.y;
		}

		/* Update button state */
		uc_priv->left_button_state = new_state;

		/* If we just detected a click, return it */
		if (pending) {
			if (xp)
				*xp = uc_priv->click_x;
			if (yp)
				*yp = uc_priv->click_y;

			return 0;
		}
	}

	return -EAGAIN;
}

UCLASS_DRIVER(mouse) = {
	.id		= UCLASS_MOUSE,
	.name		= "mouse",
	.per_device_auto = sizeof(struct mouse_uc_priv),
};
