.. SPDX-License-Identifier: GPL-2.0+

chid command
============

Synopsis
--------

::

    chid show

Description
-----------

The chid command provides access to Computer Hardware IDs (CHIDs) generated from
SMBIOS data. CHIDs are used by Windows Update and fwupd for hardware
identification and firmware-update targeting.

CHIDs are generated according to Microsoft's specification, which defines 15
different hardware ID variants (HardwareID-00 through HardwareID-14), each
using different combinations of SMBIOS fields. The variants range from most
specific (including all identifying fields) to least specific (manufacturer
only).

Subcommands
-----------

show
    Show the relevant SMBIOS values for the current board. These are used to
    calculate CHIDs.


Examples
--------

::

    => chid show
    Manufacturer:      Sandbox Corp
    Family:            Sandbox_Family
    Product Name:      Sandbox Computer
    Product SKU:       SANDBOX-SKU
    Baseboard Manuf:   Sandbox Boards
    Baseboard Product: Sandbox Motherboard
    BIOS Vendor:       U-Boot
    BIOS Version:      2025.08-g167811e037b5-dirty
    BIOS Major:        25
    BIOS Minor:        8
    Enclosure Type:    2
    =>


Configuration
-------------

The chid command is available when `CONFIG_CMD_CHID` is enabled.

Return value
------------

The return value $? is 0 (true) on success, 1 (false) on failure.

See also
--------

* :doc:`smbios <smbios>` - SMBIOS table information
