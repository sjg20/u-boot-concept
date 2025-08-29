.. SPDX-License-Identifier: GPL-2.0+

shim command
============

Synopsis
--------

::

    shim debug [[-n] <0/1>]

Description
-----------

The ``shim`` command provides utilities for controlling and configuring the
UEFI Shim bootloader when U-Boot is running as an EFI application. Shim is a
UEFI bootloader that is used to verify and boot signed operating system
loaders, commonly used in secure boot environments.

The ``shim debug`` subcommand manages the Shim verbose debugging mode through
the ``SHIM_VERBOSE`` EFI variable.

Subcommands
~~~~~~~~~~~

shim debug
^^^^^^^^^^

Controls the Shim verbose debugging mode.

**Usage:**

::

    shim debug          # Display current debug state (0 or 1)
    shim debug 0        # Disable verbose debugging
    shim debug 1        # Enable verbose debugging
    shim debug -n 1     # Enable verbose debugging (non-volatile)

The command reads from or writes to the ``SHIM_VERBOSE`` EFI variable in the
Shim Lock GUID namespace. When verbose mode is enabled (value = 1), Shim will
output additional debugging information during the boot process. When disabled
(value = 0 or variable doesn't exist), Shim operates silently.

**Parameters:**

* ``-n`` - Makes the variable non-volatile (persistent across reboots)
* ``<0/1>`` - Optional parameter to set debug mode:

  * ``0`` - Disable verbose debugging
  * ``1`` - Enable verbose debugging
  * If omitted, displays current debug state

**Return value:**

The command returns 0 on success, 1 on failure.

**Examples:**

Check current debug state::

    => shim debug
    0

Enable verbose debugging::

    => shim debug 1

Disable verbose debugging::

    => shim debug 0

Enable verbose debugging with persistence across reboots::

    => shim debug -n 1

Configuration
~~~~~~~~~~~~~

The shim command is available when:

* ``CONFIG_CMD_SHIM`` is enabled
* U-Boot is running as an EFI application
* EFI-variable support is available

Implementation Details
~~~~~~~~~~~~~~~~~~~~~~

The command uses the EFI variable services to read and write the
``SHIM_VERBOSE`` variable with the following characteristics:

* **Variable Name:** ``SHIM_VERBOSE`` (Unicode string)
* **GUID:** EFI Shim Lock GUID (``605dab50-e046-4300-abb6-3dd810dd8b23``)
* **Attributes:** ``EFI_VARIABLE_BOOTSERVICE_ACCESS`` (default) or
  ``EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE`` (with ``-n`` flag)
* **Data Type:** 32-bit unsigned integer (4 bytes)
* **Values:** 0 (disabled) or 1 (enabled)

By default, the variable is volatile and will be reset on reboot. When the
``-n`` flag is used, the variable becomes non-volatile and persists across
reboots until explicitly changed or the variable store is cleared.

See Also
~~~~~~~~

* :doc:`efidebug` - EFI debugging utilities
* :doc:`bootefi` - Boot EFI applications
* :doc:`/develop/uefi/u-boot_on_efi`
