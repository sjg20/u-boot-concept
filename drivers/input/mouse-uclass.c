// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <dm.h>
#include <errno.h>
#include <mouse.h>
#include <video.h>

int mouse_get_event(struct udevice *dev, struct mouse_event *evt)
{
	struct mouse_uc_priv *uc_priv = dev_get_uclass_priv(dev);
	struct mouse_ops *ops = mouse_get_ops(dev);
	int ret;

	if (!ops->get_event)
		return -ENOSYS;

	ret = ops->get_event(dev, evt);
	if (ret)
		return ret;

	/* Update last position for motion events */
	if (evt->type == MOUSE_EV_MOTION) {
		uc_priv->last_pos.x = evt->motion.x;
		uc_priv->last_pos.y = evt->motion.y;
	}

	/* Update last position for button events */
	if (evt->type == MOUSE_EV_BUTTON) {
		uc_priv->last_pos.x = evt->button.x;
		uc_priv->last_pos.y = evt->button.y;
	}

	return 0;
}

int mouse_get_click(struct udevice *dev, struct vid_pos *pos)
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
		bool pressed = event.button.press_state == BUTTON_PRESSED;
		bool pending = false;

		/* Detect press->release transition (click) */
		if (uc_priv->left_pressed && !pressed) {
			pending = true;
			uc_priv->click_pos.x = event.button.x;
			uc_priv->click_pos.y = event.button.y;
		}

		/* Update button state */
		uc_priv->left_pressed = pressed;

		/* If we just detected a click, return it */
		if (pending) {
			*pos = uc_priv->click_pos;
			return 0;
		}
	}

	return -EAGAIN;
}

int mouse_get_pos(struct udevice *dev, struct vid_pos *pos)
{
	struct mouse_uc_priv *uc_priv = dev_get_uclass_priv(dev);

	*pos = uc_priv->last_pos;

	return 0;
}

int mouse_set_ptr_visible(struct udevice *dev, bool visible)
{
	struct mouse_ops *ops = mouse_get_ops(dev);

	if (!ops->set_ptr_visible)
		return -ENOSYS;

	return ops->set_ptr_visible(dev, visible);
}

int mouse_set_video(struct udevice *dev, struct udevice *video_dev)
{
	struct mouse_uc_priv *uc_priv = dev_get_uclass_priv(dev);

	uc_priv->video_dev = video_dev;
	if (video_dev) {
		uc_priv->video_width = video_get_xsize(video_dev);
		uc_priv->video_height = video_get_ysize(video_dev);
	} else {
		uc_priv->video_width = 0;
		uc_priv->video_height = 0;
	}

	return 0;
}

UCLASS_DRIVER(mouse) = {
	.id		= UCLASS_MOUSE,
	.name		= "mouse",
	.per_device_auto = sizeof(struct mouse_uc_priv),
};
