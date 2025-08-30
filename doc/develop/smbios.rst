.. SPDX-License-Identifier: GPL-2.0+

SMBIOS tables
=============

The System Management BIOS (SMBIOS) table is used to deliver management
information from the firmware to the operating system. The content is
standardized in [1]_.

In Linux you can use the dmidecode command to view the contents of the SMBIOS
table.

When booting via UEFI the SMBIOS table is transferred as an UEFI configuration
table to the operating system.

To generate SMBIOS tables in U-Boot, the CONFIG_GENERATE_SMBIOS_TABLE option
must be enabled. The easiest way to provide the values to use is via the device
tree. For details see
:download:`smbios.txt <../device-tree-bindings/sysinfo/smbios.txt>`.

See also the :doc:`/usage/cmd/smbios`.

Programming Interface
---------------------

Developers can access SMBIOS information programmatically:

Functions
~~~~~~~~~

* ``smbios_locate()`` - Locate and parse SMBIOS tables
* ``smbios_get_header()`` - Get a specific SMBIOS table by type
* ``smbios_string()`` - Extract strings from SMBIOS tables

Example code::

    const struct smbios_type1 *sys;
    struct smbios_info info;
    int ret;

    ret = smbios_locate(gd_smbios_start(), &info);
    if (ret)
        return ret;

    sys = smbios_get_header(&info, SMBIOS_SYSTEM_INFORMATION);
    if (sys) {
        const char *manufacturer = smbios_string(&sys->hdr, sys->manufacturer);
        printf("Manufacturer: %s\n", manufacturer);
    }


Board-specific Implementation
-----------------------------

Boards can provide custom SMBIOS data by implementing board-specific functions:

* ``smbios_update_version()`` - Update version strings
* ``smbios_system_serial()`` - Provide system serial number
* ``smbios_system_uuid()`` - Provide system UUID
* ``smbios_system_sku()`` - Provide SKU number
* ``smbios_baseboard_serial()`` - Provide baseboard serial number

These functions are called during SMBIOS table generation if defined.


Troubleshooting
---------------

Common issues and solutions:

**Tables not generated**
  Ensure ``CONFIG_SMBIOS`` is enabled and memory allocation is sufficient

**Missing information**
  Check that board-specific functions are implemented or default configuration
  values are set

**Incorrect data**
  Verify environment variables and board-specific function implementations

**Memory layout issues**
  Check memory map and ensure SMBIOS tables don't conflict with other data


.. [1] `System Management BIOS (SMBIOS) Reference, version 3.5
   <https://www.dmtf.org/content/dmtf-releases-smbios-35>`_
