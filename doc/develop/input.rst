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
3.  Implement the driver using the keyboard uclass interface defined in
    `include/keyboard.h`. This typically involves implementing a `probe`
    function that registers the keyboard device.
4.  Add your new driver to the `Makefile` in `drivers/input/`.

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
