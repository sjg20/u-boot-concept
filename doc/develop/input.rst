.. SPDX-License-Identifier: GPL-2.0+
.. (C) Copyright 2025, Google LLC

Input subsystem
===============

This document provides an overview of the U-Boot input subsystem.

Overview
--------

The input subsystem handles input devices such as keyboards. It is based on the
U-Boot driver model and provides a uclass for keyboards. The core of the
subsystem is in `drivers/input/`.

The file `drivers/input/input.c` provides helpers to translate keycodes into
ASCII characters, handling different keyboard layouts (e.g., English, German)
and modifier keys (e.g., Shift, Ctrl). It also manages keyboard LEDs and key
repeat functionality.

The `input.h` Interface
-----------------------

The header file `include/input.h` defines the core API for the input subsystem.
The central data structure is `struct input_config`, which holds the state of an
input device, including its FIFO buffer, modifier states, and key translation
tables.

Key functions include:

- ``input_init()``: Initializes the `input_config` structure.
- ``input_add_tables()``: Adds the standard key-translation tables for different
  keyboard layouts.
- ``input_add_keycode()``: Adds or removes a keycode from the internal list of
  depressed keys. This is used by drivers that manage their own key scanning.
- ``input_send_keycodes()``: Converts a list of keycodes to ASCII and queues them
  for the console.
- ``input_tstc()``: Checks if a character is available to be read.
- ``input_getc()``: Reads a character from the input buffer.
- ``input_stdio_register()``: Registers the input device with the stdio system,
  making it available as a console device (e.g., `stdin`).

Enabling the input subsystem
----------------------------

To enable the input subsystem, you need to enable the following options in your
board's configuration file:

- ``CONFIG_DM``: Enables the driver model.
- ``CONFIG_INPUT``: Enables the input subsystem.
- ``CONFIG_DM_KEYBOARD``: Enables keyboard support within the driver model.

Adding a new keyboard driver
----------------------------

To add a new keyboard driver, you need to:

1.  Create a new driver source file in `drivers/input/`.
2.  Add a Kconfig option for your driver in `drivers/input/Kconfig`. This
    option should depend on `CONFIG_DM_KEYBOARD`.
3.  Implement the driver using the keyboard uclass interface.
4.  Add your new driver to the `Makefile` in `drivers/input/`.

Example: The Button Keyboard Driver
-----------------------------------

The `button_kbd.c` driver is a good example of a simple keyboard driver. It
maps GPIO button presses to keyboard events. Let's walk through its implementation.

1.  **Driver Probe (`button_kbd_probe`)**

    The `probe` function is the entry point for the driver. It performs the
    following steps:

    - It gets the driver's private data and the uclass private data.
    - It initializes the input system for this device by calling `input_init()`
      and `input_add_tables()`.
    - It sets the `read_keys` callback in the `input_config` structure to its
      own `button_read_keys` function. This function will be called by the
      input core when it needs to check for new key events.
    - It registers the device as a stdio device using `input_stdio_register()`,
      naming it "button-kbd". This makes it available as a potential `stdin`.

    .. code-block:: c

        static int button_kbd_probe(struct udevice *dev)
        {
            struct button_kbd_priv *priv = dev_get_priv(dev);
            struct keyboard_priv *uc_priv = dev_get_uclass_priv(dev);
            struct stdio_dev *sdev = &uc_priv->sdev;
            struct input_config *input = &uc_priv->input;
            int ret = 0;

            input_init(input, false);
            input_add_tables(input, false);

            /* Register the device. */
            priv->input = input;
            input->dev = dev;
            input->read_keys = button_read_keys;
            strcpy(sdev->name, "button-kbd");
            ret = input_stdio_register(sdev);
            if (ret) {
                debug("%s: input_stdio_register() failed\n", __func__);
                return ret;
            }

            return 0;
        }

2.  **Reading Keys (`button_read_keys`)**

    This function is the heart of the driver. It's called by the input core
    (via the function pointer set in `probe`) to poll for input.

    - It iterates through all the button devices that have been probed.
    - For each button, it gets the current state (`button_get_state()`) and
      compares it to the previous state.
    - If the state has changed, it means a button has been pressed or released.
    - It then calls `input_add_keycode()` with the button's assigned keycode
      and the new state (pressed or released). The input core then handles
      the keycode, translates it to ASCII if necessary, and places it in the
      console buffer.

    .. code-block:: c

        int button_read_keys(struct input_config *input)
        {
            struct button_kbd_priv *priv = dev_get_priv(input->dev);
            struct udevice *button_gpio_devp;
            struct uclass *uc;
            int i = 0;
            u32 code, state, state_changed = 0;

            uclass_id_foreach_dev(UCLASS_BUTTON, button_gpio_devp, uc) {
                ...
                code = button_get_code(button_gpio_devp);
                ...
                state = button_get_state(button_gpio_devp);
                state_changed = state != priv->old_state[i];

                if (state_changed) {
                    debug("%s: %d\n", uc_plat->label, code);
                    priv->old_state[i] = state;
                    input_add_keycode(input, code, state);
                }
                i++;
            }
            return 0;
        }

3.  **Driver Registration**

    Finally, the driver is registered with the U-Boot driver model using the
    `U_BOOT_DRIVER` macro, specifying its name, uclass ID (`UCLASS_KEYBOARD`),
    and probe function.

    .. code-block:: c

        U_BOOT_DRIVER(button_kbd) = {
            .name       = "button_kbd",
            .id         = UCLASS_KEYBOARD,
            .ops        = &button_kbd_ops,
            .priv_auto  = sizeof(struct button_kbd_priv),
            .probe      = button_kbd_probe,
        };

Available drivers
-----------------

The following keyboard drivers are available:

- `apple_spi_kbd`: Keyboards on Apple SoCs using a HID-over-SPI protocol.
- `button_kbd`: Maps GPIO buttons to keycode events.
- `cros_ec_keyb`: Keyboards on Chrome OS devices using an Embedded Controller (EC).
- `hid_i2c`: HID-over-I2C devices.
- `i8042`: i8042 keyboard controller.
- `key_matrix`: Matrix keyboards.
- `tegra-kbc`: NVIDIA Tegra internal matrix keyboard controller.
- `twl4030`: TWL4030 input controller.
