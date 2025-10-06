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

/* Maximum coordinate value for mouse position */
#define MOUSE_MAX_COORD 0xffff

/**
 * struct efi_mouse_priv - Private data for EFI mouse driver
 *
 * @simple: Simple pointer protocol (relative movement)
 * @simple_last: Last simple pointer state
 * @has_last_state: True if we have a previous state for delta calculation
 * @x: Current X position
 * @y: Current Y position
 * @buttons: Current button state
 * @old_buttons: Previous button state for detecting changes
 * @timer_event: EFI timer event for periodic polling
 */
struct efi_mouse_priv {
	struct efi_simple_pointer_protocol *simple;
	struct efi_simple_pointer_state simple_last;
	bool has_last_state;
	int x, y;
	int buttons;
	int old_buttons;
	struct efi_event *timer_event;
};

/**
 * get_button_event() - Check for button-change events
 *
 * @priv: Private data
 * @new_buttons: New button state
 * @event: Event to populate if button changed
 * Return: 0 if button event found, -EAGAIN if no button change
 */
static int get_button_event(struct efi_mouse_priv *priv, int new_buttons,
			    struct mouse_event *event)
{
	struct mouse_button *but = &event->button;
	int diff = new_buttons ^ priv->old_buttons;
	int i;

	if (new_buttons == priv->old_buttons)
		return -EAGAIN;

	event->type = MOUSE_EV_BUTTON;
	/* Find first changed button */
	for (i = 0; i < 2; i++) {
		u8 mask = 1 << i;

		if (!(diff & mask))
			continue;

		but->button = i;
		but->pressed = (new_buttons & mask) ? true : false;
		but->clicks = 1;
		but->x = priv->x;
		but->y = priv->y;
		priv->old_buttons ^= mask;
		return 0;
	}

	return -EAGAIN;
}

static int efi_mouse_get_event(struct udevice *dev, struct mouse_event *event)
{
	struct efi_mouse_priv *priv = dev_get_priv(dev);
	struct efi_simple_pointer_state state;
	efi_status_t ret;
	int new_buttons;
	if (!priv->simple)
		return -ENODEV;

	/* Use timer-based polling approach like EFI keyboard */
	if (priv->timer_event) {
		struct efi_boot_services *boot = efi_get_boot();
		efi_uintn_t index;
		struct efi_event *events[2];
		efi_uintn_t num_events = 1;

		events[0] = priv->timer_event;
		if (priv->simple->wait_for_input) {
			events[1] = priv->simple->wait_for_input;
			num_events = 2;
		}

		ret = boot->wait_for_event(num_events, events, &index);
		if (ret != EFI_SUCCESS)
			return -EAGAIN;
	}

	/* Get current pointer state */
	ret = priv->simple->get_state(priv->simple, &state);
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
	ret = get_button_event(priv, new_buttons, event);
	if (!ret)
		return 0;

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

/**
 * setup_simple_pointer() - Set up simple pointer protocol
 *
 * @priv: Private data
 * Return: 0 if OK, -ve on error
 */
static int setup_simple_pointer(struct efi_mouse_priv *priv)
{
	struct efi_boot_services *boot = efi_get_boot();
	efi_handle_t *handles;
	efi_uintn_t num_handles;
	efi_status_t ret;

	log_debug("EFI simple-pointer mouse probe\n");
	ret = boot->locate_handle_buffer(BY_PROTOCOL, &efi_guid_simple_pointer,
					 NULL, &num_handles, &handles);
	if (ret)
		return -ENODEV;

	log_debug("Found %zu simple pointer device(s)\n", num_handles);

	/* Use the first simple pointer device */
	ret = boot->open_protocol(handles[0], &efi_guid_simple_pointer,
				   (void **)&priv->simple,
				   efi_get_parent_image(), NULL,
				   EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (ret) {
		log_debug("Cannot open simple pointer protocol (ret=0x%lx)\n",
			  ret);
		efi_free_pool(handles);
		return -EIO;
	}

	log_debug("Using simple pointer protocol\n");
	efi_free_pool(handles);

	return 0;
}

static int efi_mouse_probe(struct udevice *dev)
{
	struct efi_mouse_priv *priv = dev_get_priv(dev);
	struct efi_boot_services *boot = efi_get_boot();
	efi_status_t ret;

	if (setup_simple_pointer(priv))
		return -ENODEV;

	/* Reset the pointer device */
	ret = priv->simple->reset(priv->simple, false);
	if (ret != EFI_SUCCESS) {
		log_warning("Failed to reset EFI pointer device\n");
		/* Continue anyway - some devices might not support reset */
	}

	/* Create a timer event for periodic checking */
	ret = boot->create_event(EVT_TIMER, TPL_NOTIFY, NULL, NULL,
				&priv->timer_event);
	if (ret) {
		log_debug("Failed to create timer event (ret=0x%lx)\n", ret);
		/* Continue without timer - fallback to direct polling */
		priv->timer_event = NULL;
	} else {
		/* Set timer to trigger every 10ms (100000 x 100ns = 10ms) */
		ret = boot->set_timer(priv->timer_event, EFI_TIMER_PERIODIC,
				      10000);
		if (ret) {
			log_debug("Failed to set timer (ret=0x%lx)\n", ret);
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

	if (priv->timer_event)
		boot->close_event(priv->timer_event);

	/* Protocol will be automatically closed when the image is unloaded */

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
