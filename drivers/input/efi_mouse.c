// SPDX-License-Identifier: GPL-2.0+
/*
 * EFI mouse driver using Simple Pointer Protocol
 *
 * Copyright 2025 Google LLC
 * Written by Claude <noreply@anthropic.com>
 */

#define LOG_CATEGORY UCLASS_MOUSE

#include <dm.h>
#include <efi.h>
#include <efi_api.h>
#include <log.h>
#include <mouse.h>

struct efi_mouse_priv {
	struct efi_simple_pointer_protocol *pointer;
	struct efi_simple_pointer_state last_state;
	bool has_last_state;
	int x, y;
	int buttons;
	int old_buttons;
	struct efi_event *timer_event;
};

static int efi_mouse_get_event(struct udevice *dev, struct mouse_event *event)
{
	struct efi_mouse_priv *priv = dev_get_priv(dev);
	struct efi_simple_pointer_state state;
	efi_status_t ret;
	int new_buttons;
	if (!priv->pointer)
		return -ENODEV;

	/* Use timer-based polling approach like EFI keyboard */
	if (priv->timer_event) {
		struct efi_boot_services *boot = efi_get_boot();
		efi_uintn_t index;
		struct efi_event *events[2];
		efi_uintn_t num_events = 1;

		events[0] = priv->timer_event;
		if (priv->pointer->wait_for_input) {
			events[1] = priv->pointer->wait_for_input;
			num_events = 2;
		}

		ret = boot->wait_for_event(num_events, events, &index);
		if (ret != EFI_SUCCESS)
			return -EAGAIN;
	}

	/* Get current pointer state */
	ret = priv->pointer->get_state(priv->pointer, &state);
	if (ret != EFI_SUCCESS) {
		if (ret == EFI_NOT_READY)
			return -EAGAIN;
		printf("EFI mouse: get_state failed (ret=0x%lx)\n", ret);
		return -EIO;
	}

	/* Check for button changes */
	new_buttons = 0;
	if (state.left_button)
		new_buttons |= 1 << 0;
	if (state.right_button)
		new_buttons |= 1 << 1;

	if (new_buttons != priv->old_buttons) {
		struct mouse_button *but = &event->button;
		u8 diff = new_buttons ^ priv->old_buttons;
		int i;

		event->type = MOUSE_EV_BUTTON;
		/* Find first changed button */
		for (i = 0; i < 2; i++) {
			u8 mask = 1 << i;
			if (diff & mask) {
				but->button = i;
				but->press_state = (new_buttons & mask) ? 1 : 0;
				but->clicks = 1;
				but->x = priv->x;
				but->y = priv->y;
				priv->old_buttons ^= mask;
				return 0;
			}
		}
	}

	/* Check for movement */
	if (state.relative_movement_x || state.relative_movement_y) {
		struct mouse_motion *motion = &event->motion;

		/* Update absolute position */
		priv->x += state.relative_movement_x;
		priv->x = max(priv->x, 0);
		priv->x = min(priv->x, 0xffff);

		priv->y += state.relative_movement_y;
		priv->y = max(priv->y, 0);
		priv->y = min(priv->y, 0xffff);

		event->type = MOUSE_EV_MOTION;
		motion->state = new_buttons;
		motion->x = priv->x;
		motion->y = priv->y;
		motion->xrel = state.relative_movement_x;
		motion->yrel = state.relative_movement_y;

		priv->buttons = new_buttons;
		return 0;
	}

	priv->buttons = new_buttons;

	return -EAGAIN;
}

static int efi_mouse_probe(struct udevice *dev)
{
	struct efi_mouse_priv *priv = dev_get_priv(dev);
	struct efi_boot_services *boot = efi_get_boot();
	efi_status_t ret;
	efi_handle_t *handles;
	efi_uintn_t num_handles;

	log_debug("EFI mouse probe\n");

	/* Find Simple Pointer Protocol handles */
	ret = boot->locate_handle_buffer(BY_PROTOCOL, &efi_guid_simple_pointer,
					 NULL, &num_handles, &handles);
	if (ret != EFI_SUCCESS) {
		printf("EFI mouse: No EFI pointer devices found (ret=0x%lx)\n", ret);
		return -ENODEV;
	}

	log_debug("Found %zu EFI pointer device(s)\n", num_handles);

	/* Use the first pointer device */
	ret = boot->open_protocol(handles[0], &efi_guid_simple_pointer,
				(void **)&priv->pointer, efi_get_parent_image(),
				NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (ret != EFI_SUCCESS) {
		printf("EFI mouse: Failed to open EFI pointer protocol (ret=0x%lx)\n", ret);
		efi_free_pool(handles);
		return -ENODEV;
	}

	efi_free_pool(handles);

	/* Reset the pointer device */
	ret = priv->pointer->reset(priv->pointer, false);
	if (ret != EFI_SUCCESS) {
		log_warning("Failed to reset EFI pointer device\n");
		/* Continue anyway - some devices might not support reset */
	}

	priv->x = 0;
	priv->y = 0;
	priv->buttons = 0;
	priv->old_buttons = 0;
	priv->has_last_state = false;

	/* Create a timer event for periodic checking */
	ret = boot->create_event(EVT_TIMER, TPL_NOTIFY, NULL, NULL,
				&priv->timer_event);
	if (ret != EFI_SUCCESS) {
		printf("EFI mouse: Failed to create timer event (ret=0x%lx)\n", ret);
		/* Continue without timer - fallback to direct polling */
		priv->timer_event = NULL;
	} else {
		/* Set timer to trigger every 10ms (100000 x 100ns = 10ms) */
		ret = boot->set_timer(priv->timer_event, EFI_TIMER_PERIODIC, 10000);
		if (ret != EFI_SUCCESS) {
			printf("EFI mouse: Failed to set timer (ret=0x%lx)\n", ret);
			boot->close_event(priv->timer_event);
			priv->timer_event = NULL;
		}
	}

	log_info("EFI mouse initialized\n");
	return 0;
}

static int efi_mouse_remove(struct udevice *dev)
{
	struct efi_mouse_priv *priv = dev_get_priv(dev);
	struct efi_boot_services *boot = efi_get_boot();

	if (priv->timer_event) {
		boot->close_event(priv->timer_event);
		priv->timer_event = NULL;
	}

	if (priv->pointer) {
		/* Protocol will be automatically closed when the image is unloaded */
		priv->pointer = NULL;
	}

	return 0;
}

static const struct mouse_ops efi_mouse_ops = {
	.get_event	= efi_mouse_get_event,
};

static const struct udevice_id efi_mouse_ids[] = {
	{ .compatible = "efi,mouse" },
	{ }
};

U_BOOT_DRIVER(efi_mouse) = {
	.name	= "efi_mouse",
	.id	= UCLASS_MOUSE,
	.of_match = efi_mouse_ids,
	.ops	= &efi_mouse_ops,
	.probe = efi_mouse_probe,
	.remove = efi_mouse_remove,
	.priv_auto	= sizeof(struct efi_mouse_priv),
};
