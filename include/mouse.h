/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Mouse/trackpad/touchscreen input uclass
 *
 * Copyright 2020 Google LLC
 */

#ifndef _MOUSE_H
#define _MOUSE_H

#include <stdbool.h>
#include <video_defs.h>

struct udevice;

enum mouse_ev_t {
	MOUSE_EV_NULL,
	MOUSE_EV_MOTION,
	MOUSE_EV_BUTTON,
};

enum mouse_state_t {
	BUTTON_LEFT		= 1 << 0,
	BUTTON_MIDDLE		= 1 << 1,
	BUTTON_RIGHT		= 1 << 2,
	BUTTON_SCROLL_PLUS	= 1 << 3,
	BUTTON_SCROLL_MINUS	= 1 << 4,
};

enum mouse_press_state_t {
	BUTTON_RELEASED		= 0,
	BUTTON_PRESSED,
};

/**
 * struct mouse_uc_priv - private data for mouse uclass
 *
 * @left_button_state: Current state of left button (BUTTON_PRESSED/BUTTON_RELEASED)
 * @click_pos: Position where the click occurred
 * @last_pos: Last position received from mouse
 * @video_dev: Video device for coordinate scaling
 * @video_width: Width of video display
 * @video_height: Height of video display
 */
struct mouse_uc_priv {
	enum mouse_press_state_t left_button_state;
	struct vid_pos click_pos;
	struct vid_pos last_pos;
	struct udevice *video_dev;
	int video_width;
	int video_height;
};

/**
 * struct mouse_event - information about a mouse event
 *
 * @type: Mouse event ype
 */
struct mouse_event {
	enum mouse_ev_t type;
	union {
		/**
		 * @state: Mouse state (enum mouse_state_t bitmask)
		 * @x: X position of mouse
		 * @y: Y position of mouse
		 * @xrel: Relative motion in X direction
		 * @yrel: Relative motion in Y direction
		 */
		struct mouse_motion {
			unsigned char state;
			unsigned short x;
			unsigned short y;
			short xrel;
			short yrel;
		} motion;

		/**
		 * @button: Button number that was pressed/released (BUTTON_...)
		 * @state: BUTTON_PRESSED / BUTTON_RELEASED
		 * @clicks: number of clicks (normally 1; 2 = double-click)
		 * @x: X position of mouse
		 * @y: Y position of mouse
		 */
		struct mouse_button {
			unsigned char button;
			unsigned char press_state;
			unsigned char clicks;
			unsigned short x;
			unsigned short y;
		} button;
	};
};

struct mouse_ops {
	/**
	 * mouse_get_event() - Get a mouse event
	 *
	 * Gets the next available mouse event from the device. This can be a
	 * motion event (mouse movement) or a button event (button press or
	 * release).
	 *
	 * @dev: Mouse device
	 * @event: Returns the mouse event
	 * Returns: 0 if OK, -EAGAIN if no event available, -ENOSYS if not
	 * supported
	 */
	int (*get_event)(struct udevice *dev, struct mouse_event *event);

	/**
	 * set_ptr_visible() - Show or hide the system mouse pointer
	 *
	 * This is used to hide the system pointer when expo is rendering its
	 * own custom mouse pointer.
	 *
	 * @dev: Mouse device
	 * @visible: true to show the pointer, false to hide it
	 * Returns: 0 if OK, -ENOSYS if not supported
	 */
	int (*set_ptr_visible)(struct udevice *dev, bool visible);
};

#define mouse_get_ops(dev)	((struct mouse_ops *)(dev)->driver->ops)

/**
 * mouse_get_event() - Get a mouse event
 *
 * Gets the next available mouse event from the device. This can be a
 * motion event (mouse movement) or a button event (button press or
 * release).
 *
 * @dev: Mouse device
 * @event: Returns the mouse event
 * Returns: 0 if OK, -EAGAIN if no event available, -ENOSYS if not
 * supported
 */
int mouse_get_event(struct udevice *dev, struct mouse_event *event);

/**
 * mouse_get_click() - Check if a left mouse button click has occurred
 *
 * @dev: Mouse device
 * @pos: Returns position of click
 * Returns: 0 if a click has occurred, -EAGAIN if no click pending
 */
int mouse_get_click(struct udevice *dev, struct vid_pos *pos);

/**
 * mouse_get_pos() - Get the current mouse position
 *
 * @dev: Mouse device
 * @pos: Returns last position
 * Returns: 0 if position is available, -ve on error
 */
int mouse_get_pos(struct udevice *dev, struct vid_pos *pos);

/**
 * mouse_set_ptr_visible() - Show or hide the system mouse pointer
 *
 * This is used to hide the system pointer when rendering a custom mouse
 * pointer (e.g., in expo mode).
 *
 * @dev: Mouse device
 * @visible: true to show the pointer, false to hide it
 * Returns: 0 if OK, -ENOSYS if not supported
 */
int mouse_set_ptr_visible(struct udevice *dev, bool visible);

/**
 * mouse_set_video() - Set the video device for coordinate scaling
 *
 * Sets up the video device in the mouse uclass private data so mouse drivers
 * can scale coordinates to match the display resolution.
 *
 * @dev: Mouse device
 * @video_dev: Video device
 * Returns: 0 if OK, -ve on error
 */
int mouse_set_video(struct udevice *dev, struct udevice *video_dev);

#endif
