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

	/* Update last position for button events and detect clicks */
	if (evt->type == MOUSE_EV_BUTTON) {
		uc_priv->last_pos.x = evt->button.x;
		uc_priv->last_pos.y = evt->button.y;

		/* Process left-button clicks */
		if (evt->button.button == BUTTON_LEFT) {
			/* Detect press->release transition (click) */
			if (uc_priv->left_pressed && !evt->button.pressed) {
				uc_priv->click_pending = true;
				uc_priv->click_pos.x = evt->button.x;
				uc_priv->click_pos.y = evt->button.y;
			}

			/* Update button state */
			uc_priv->left_pressed = evt->button.pressed;
		}
	}

	return 0;
}

int mouse_get_click(struct udevice *dev, struct vid_pos *pos)
{
	struct mouse_uc_priv *uc_priv = dev_get_uclass_priv(dev);
	struct mouse_event event;

	/* Process all available events until we find a click */
	while (true) {
		if (uc_priv->click_pending) {
			*pos = uc_priv->click_pos;
			uc_priv->click_pending = false;
			break;
		}

		if (mouse_get_event(dev, &event))
			return -EAGAIN;  /* No more events */
	}

	return 0;
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
		uc_priv->last_pos.x = uc_priv->video_width / 2;
		uc_priv->last_pos.y = uc_priv->video_height / 2;
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
