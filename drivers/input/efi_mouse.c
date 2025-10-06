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
#include <video.h>

/* Maximum coordinate value for mouse position */
#define MOUSE_MAX_COORD 0xffff

/**
 * struct efi_mouse_priv - Private data for EFI mouse driver
 *
 * @simple: Simple pointer protocol (relative movement)
 * @abs: Absolute pointer protocol (absolute position)
 * @simple_last: Last simple pointer state
 * @abs_last: Last absolute pointer state
 * @has_last_state: True if we have a previous state for delta calculation
 * @use_absolute: True to use absolute pointer, false for simple/relative
 * @x: Current X position
 * @y: Current Y position
 * @buttons: Current button state
 * @old_buttons: Previous button state for detecting changes
 * @timer_event: EFI timer event for periodic polling
 */
struct efi_mouse_priv {
	struct efi_simple_pointer_protocol *simple;
	struct efi_absolute_pointer_protocol *abs;
	struct efi_simple_pointer_state simple_last;
	struct efi_absolute_pointer_state abs_last;
	bool has_last_state;
	bool use_absolute;
	int x, y;
	int buttons;
	int old_buttons;
	struct efi_event *timer_event;
};

/**
 * get_abs_pointer() - Handle absolute pointer input
 *
 * @priv: Private data
 * @uc_priv: Uclass-private data
 * @rel_x: Returns relative X movement
 * @rel_y: Returns relative Y movement
 * @new_buttons: Returns button state
 * Return: 0 if OK, -EAGAIN if no event, -ve on error
 */
static int get_abs_pointer(struct efi_mouse_priv *priv,
			   struct mouse_uc_priv *uc_priv, int *rel_x,
			   int *rel_y, int *new_buttons)
{
	struct efi_absolute_pointer_state state;
	efi_status_t ret;

	/* Debug: check structure size and alignment */
	log_debug("State struct size: %zu, address: %p\n", sizeof(state),
		  &state);

	ret = priv->abs->get_state(priv->abs, &state);
	if (ret == EFI_NOT_READY)
		return -EAGAIN;
	if (ret) {
		log_debug("abs: get_state failed (ret=0x%lx)\n", ret);
		return -EIO;
	}

	/* Always log the state values to see what we're getting */
	log_debug("abs: X=%llu Y=%llu Buttons=0x%x\n", state.current_x,
		  state.current_y, state.active_buttons);

	/* Calculate relative movement */
	if (priv->has_last_state) {
		*rel_x = (int)(state.current_x - priv->abs_last.current_x);
		*rel_y = (int)(state.current_y - priv->abs_last.current_y);
		log_debug("abs: rel_x=%d, rel_y=%d\n", *rel_x, *rel_y);
	}
	priv->abs_last = state;

	/* Update absolute position - scale to video display if available */
	if (uc_priv->video_dev && priv->abs->mode) {
		struct efi_absolute_pointer_mode *mode = priv->abs->mode;
		u64 x_range = mode->abs_max_x - mode->abs_min_x;
		u64 y_range = mode->abs_max_y - mode->abs_min_y;

		if (x_range > 0 && y_range > 0) {
			log_debug("abs: unscaled x=%llx y=%llx\n",
				  state.current_x, state.current_y);
			priv->x = ((state.current_x - mode->abs_min_x) *
				   uc_priv->video_width) / x_range;
			priv->y = ((state.current_y - mode->abs_min_y) *
				   uc_priv->video_height) / y_range;
		} else {
			priv->x = state.current_x;
			priv->y = state.current_y;
		}
	} else {
		priv->x = state.current_x;
		priv->y = state.current_y;
	}

	/* Extract button state */
	*new_buttons = state.active_buttons & 0x3; /* Left and right buttons */

	return 0;
}

/**
 * get_rel_pointer() - Handle relative pointer input
 *
 * @priv: Private data
 * @rel_x: Returns relative X movement
 * @rel_y: Returns relative Y movement
 * @new_buttons: Returns button state
 * Return: 0 if OK, -EAGAIN if no event, -ve on error
 */
static int get_rel_pointer(struct efi_mouse_priv *priv, int *rel_x,
			   int *rel_y, int *new_buttons)
{
	struct efi_simple_pointer_state state;
	efi_status_t ret;

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

	log_debug("rel: calling get_state\n");
	ret = priv->simple->get_state(priv->simple, &state);
	log_debug("rel: get_state returned 0x%lx\n", ret);
	if (ret == EFI_NOT_READY)
		return -EAGAIN;
	if (ret) {
		log_debug("rel: get_state failed (ret=0x%lx)\n", ret);
		return -EIO;
	}

	log_debug("rel: RelX=%d RelY=%d LeftBtn=%d RightBtn=%d\n",
		  state.relative_movement_x, state.relative_movement_y,
		  state.left_button, state.right_button);

	/*
	 * Scale down large movement values that seem to be incorrectly
	 * reported
	 */
	*rel_x = state.relative_movement_x;
	*rel_y = state.relative_movement_y;

	/* If movement values are very large, scale them down */
	if (abs(*rel_x) > 1000) {
		*rel_x = *rel_x / 1000;
		if (*rel_x == 0 && state.relative_movement_x != 0)
			*rel_x = (state.relative_movement_x > 0) ? 1 : -1;
	}
	if (abs(*rel_y) > 1000) {
		*rel_y = *rel_y / 1000;
		if (*rel_y == 0 && state.relative_movement_y != 0)
			*rel_y = (state.relative_movement_y > 0) ? 1 : -1;
	}

	log_debug("rel: scaled RelX=%d RelY=%d\n", *rel_x, *rel_y);

	/* Update absolute position */
	priv->x += *rel_x;
	priv->x = max(priv->x, 0);
	priv->x = min(priv->x, MOUSE_MAX_COORD);

	priv->y += *rel_y;
	priv->y = max(priv->y, 0);
	priv->y = min(priv->y, MOUSE_MAX_COORD);

	/* Extract button state */
	*new_buttons = 0;
	if (state.left_button)
		*new_buttons |= 1 << 0;
	if (state.right_button)
		*new_buttons |= 1 << 1;

	return 0;
}

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
	struct mouse_uc_priv *uc_priv = dev_get_uclass_priv(dev);
	struct efi_mouse_priv *priv = dev_get_priv(dev);
	struct mouse_motion *motion;
	int new_buttons;
	int rel_x, rel_y;
	int ret;

	/*
	 * Get current pointer state. Under QEMU, EFI pointer-events are broken
	 * so we poll directly
	 */
	if (priv->use_absolute)
		ret = get_abs_pointer(priv, uc_priv, &rel_x, &rel_y,
				      &new_buttons);
	else
		ret = get_rel_pointer(priv, &rel_x, &rel_y, &new_buttons);
	if (ret)
		return ret;

	priv->has_last_state = true;

	/* Check for button changes */
	ret = get_button_event(priv, new_buttons, event);
	if (!ret)
		return 0;

	/* If there's no movement, nothing to do */
	if (!rel_x && !rel_y) {
		priv->buttons = new_buttons;
		return -EAGAIN;
	}

	motion = &event->motion;
	event->type = MOUSE_EV_MOTION;
	motion->state = new_buttons;
	motion->x = priv->x;
	motion->y = priv->y;
	motion->xrel = rel_x;
	motion->yrel = rel_y;
	priv->buttons = new_buttons;

	return 0;
}

/**
 * setup_abs_pointer() - Set up absolute pointer protocol
 *
 * @priv: Private data
 * Return: 0 if OK, -ve on error
 */
static int setup_abs_pointer(struct efi_mouse_priv *priv)
{
	struct efi_boot_services *boot = efi_get_boot();
	efi_handle_t *handles;
	efi_uintn_t num_handles;
	efi_status_t ret;

	ret = boot->locate_handle_buffer(BY_PROTOCOL,
					 &efi_guid_absolute_pointer,
					 NULL, &num_handles, &handles);
	if (ret)
		return -ENODEV;

	log_debug("Found %zu absolute pointer device(s) guid %pU\n",
		  num_handles, &efi_guid_absolute_pointer);

	/* Use the first absolute pointer device */
	ret = boot->open_protocol(handles[0], &efi_guid_absolute_pointer,
				   (void **)&priv->abs,
				   efi_get_parent_image(), NULL,
				   EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (ret) {
		log_debug("Cannot open absolute pointer protocol (ret=0x%lx)\n",
			  ret);
		efi_free_pool(handles);
		return -EIO;
	}

	priv->use_absolute = true;
	log_debug("Using absolute pointer protocol\n");
	efi_free_pool(handles);

	return 0;
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

	priv->use_absolute = false;
	log_debug("Using simple pointer protocol\n");
	efi_free_pool(handles);

	return 0;
}

static int efi_mouse_probe(struct udevice *dev)
{
	struct efi_mouse_priv *priv = dev_get_priv(dev);
	struct efi_boot_services *boot = efi_get_boot();
	efi_status_t ret;

	/* Try absolute pointer first, then fall back to simple pointer */
	if (setup_abs_pointer(priv) && setup_simple_pointer(priv))
		return -ENODEV;

	/* Reset the pointer device */
	if (priv->use_absolute)
		ret = priv->abs->reset(priv->abs, true);
	else
		ret = priv->simple->reset(priv->simple, true);
	if (ret) {
		log_warning("Failed to reset device (err=0x%lx)\n", ret);
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

	/* Test protocol validity */
	if (priv->use_absolute && priv->abs->mode) {
		struct efi_absolute_pointer_mode *mode = priv->abs->mode;

		log_debug("absolute mouse mode: x %llx-%llx y %llx-%llx\n",
			  mode->abs_min_x, mode->abs_max_x,
			  mode->abs_min_y, mode->abs_max_y);
		log_debug("absolute mouse wait_for_input event: %p\n",
			  priv->abs->wait_for_input);
	} else if (!priv->use_absolute && priv->simple) {
		log_debug("simple mouse wait_for_input event: %p\n",
			  priv->simple->wait_for_input);
	}

	log_debug("initialized (%s protocol)\n",
		  priv->use_absolute ? "absolute" : "simple");
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
