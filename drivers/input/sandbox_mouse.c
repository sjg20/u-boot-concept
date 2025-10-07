// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2020 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <dm.h>
#include <mouse.h>
#include <asm/sdl.h>

/**
 * struct sandbox_mouse_priv - Private data for sandbox mouse driver
 *
 * @test_mode: true to use test mode (inject events), false to use SDL
 * @test_event: Event to return when in test mode
 * @test_event_pending: true if test_event is pending
 * @ptr_visible: Current visibility state of mouse pointer
 */
struct sandbox_mouse_priv {
	bool test_mode;
	struct mouse_event test_event;
	bool test_event_pending;
	bool ptr_visible;
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
	struct sandbox_mouse_priv *priv = dev_get_priv(dev);

	priv->ptr_visible = visible;
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

/**
 * sandbox_mouse_get_ptr_visible() - Get pointer visibility state
 *
 * @dev: Mouse device
 * Return: true if pointer is visible, false if hidden
 */
bool sandbox_mouse_get_ptr_visible(struct udevice *dev)
{
	struct sandbox_mouse_priv *priv = dev_get_priv(dev);

	return priv->ptr_visible;
}

U_BOOT_DRIVER(mouse_sandbox) = {
	.name	= "mouse_sandbox",
	.id	= UCLASS_MOUSE,
	.of_match = mouse_sandbox_ids,
	.ops	= &mouse_sandbox_ops,
	.priv_auto = sizeof(struct sandbox_mouse_priv),
};
