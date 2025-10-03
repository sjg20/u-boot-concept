// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2020 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <dm.h>
#include <mouse.h>
#include <asm/sdl.h>

struct sandbox_mouse_priv {
	bool test_mode;
	struct mouse_event test_event;
	bool test_event_pending;
};

static int mouse_sandbox_get_event(struct udevice *dev,
				   struct mouse_event *event)
{
	struct sandbox_mouse_priv *priv = dev_get_priv(dev);
	int ret;

	/* If in test mode, return test event if pending */
	if (priv->test_mode) {
		if (priv->test_event_pending) {
			*event = priv->test_event;
			priv->test_event_pending = false;
			return 0;
		} else {
			return -EAGAIN;
		}
	}

	ret = sandbox_sdl_get_mouse_event(event);

	return ret;
}

static int mouse_sandbox_set_ptr_visible(struct udevice *dev, bool visible)
{
	sandbox_sdl_set_cursor_visible(visible);

	return 0;
}

const struct mouse_ops mouse_sandbox_ops = {
	.get_event	= mouse_sandbox_get_event,
	.set_ptr_visible = mouse_sandbox_set_ptr_visible,
};

static const struct udevice_id mouse_sandbox_ids[] = {
	{ .compatible = "sandbox,mouse" },
	{ }
};

/**
 * sandbox_mouse_set_test_mode() - Enable/disable test mode
 *
 * @dev: Mouse device
 * @test_mode: true to enable test mode, false to use SDL
 */
void sandbox_mouse_set_test_mode(struct udevice *dev, bool test_mode)
{
	struct sandbox_mouse_priv *priv = dev_get_priv(dev);

	priv->test_mode = test_mode;
	priv->test_event_pending = false;
}

/**
 * sandbox_mouse_inject() - Inject a mouse event for testing
 *
 * @dev: Mouse device (must be in test mode)
 * @event: Event to inject
 */
void sandbox_mouse_inject(struct udevice *dev, struct mouse_event *event)
{
	struct sandbox_mouse_priv *priv = dev_get_priv(dev);

	if (priv->test_mode) {
		priv->test_event = *event;
		priv->test_event_pending = true;
	}
}

U_BOOT_DRIVER(mouse_sandbox) = {
	.name	= "mouse_sandbox",
	.id	= UCLASS_MOUSE,
	.of_match = mouse_sandbox_ids,
	.ops	= &mouse_sandbox_ops,
	.priv_auto = sizeof(struct sandbox_mouse_priv),
};
