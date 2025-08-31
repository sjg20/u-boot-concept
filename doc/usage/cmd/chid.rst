.. SPDX-License-Identifier: GPL-2.0+

chid command
============

Synopsis
--------

::

    chid list
    chid show
    chid detail <variant>

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

list
    Display all 15 CHID variants with their generated UUIDs

detail <variant>
    Display details for a specific CHID variant (0-14), including which
    SMBIOS fields are used and the generated UUID

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

This shows how to obtain the CHIDs for all variants::

    => chid list
    HardwareID-00: 133e2a0f-2299-5874-b475-40f5a1744a35
    HardwareID-01: a9648aa2-bb0e-5e53-95d7-432ac18f041f
    HardwareID-02: cbb458c8-4d1a-5898-9a2c-6657c6664a9a
    HardwareID-03: a07db1d8-e0f9-5d47-a507-8c358eb7edf4
    HardwareID-04: 5efd2af9-23d6-5fe6-bdb0-e040b9e5b054
    HardwareID-05: 48aede6f-65db-51a5-8905-fdabdbc0685e
    HardwareID-06: d1c25f0a-6eb8-5c23-961c-e46694384fa0
    HardwareID-07: 84398dad-5de1-553b-87b9-1b7c4b02c505
    HardwareID-08: ed1fa480-f06b-52ba-b203-56562c52c7e2
    HardwareID-09: c0185db1-6111-5432-955a-e5ecdac0d351
    HardwareID-10: 6b7ad4ae-1f41-52dd-8499-ab9ab9e8688c
    HardwareID-11: 72523836-a3fc-57b0-8d3c-4ce56cbf6b96
    HardwareID-12: e93b37fb-5592-55dd-8e4b-9df3fbc9abde
    HardwareID-13: d67e233f-5cad-541c-87f8-ee9bca569e5e
    HardwareID-14: 45c5e2e7-db48-556b-aae4-0a03c5a15eae

Show details for a specific variant::

    => chid detail 14
    HardwareID-14: 45c5e2e7-db48-556b-aae4-0a03c5a15eae
    Fields: Manufacturer

Configuration
-------------

The chid command is available when `CONFIG_CMD_CHID` is enabled.

Return value
------------

The return value $? is 0 (true) on success, 1 (false) on failure.

See also
--------

* :doc:`smbios <smbios>` - SMBIOS table information
