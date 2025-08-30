.. SPDX-License-Identifier: GPL-2.0+

SMBIOS support in U-Boot
========================

U-Boot supports generating and using SMBIOS (System Management BIOS) tables,
which provide standardized hardware and firmware information to the operating
system and applications.

Overview
--------

SMBIOS is a standard developed by the Desktop Management Task Force (DMTF) that
defines a way for firmware to present hardware information to software running
on the system. The information is organized into typed data structures called
SMBIOS tables or structures.

SMBIOS Generation in U-Boot
---------------------------

U-Boot can automatically generate SMBIOS tables based on configuration and
hardware detection. The tables are created during the boot process and made
available to the operating system.

See also the developer documentation: :doc:`/develop/smbios`.

Configuration
~~~~~~~~~~~~~

SMBIOS support is enabled through several Kconfig options:

* ``CONFIG_SMBIOS`` - Enable SMBIOS table generation
* ``CONFIG_SMBIOS_PARSER`` - Enable SMBIOS table parsing support
* ``CONFIG_CMD_SMBIOS`` - Enable the :doc:`/usage/cmd/smbios`

Default Values
~~~~~~~~~~~~~~

Many SMBIOS fields can be configured with default values:

* ``CONFIG_SMBIOS_MANUFACTURER`` - System manufacturer name
* ``CONFIG_SMBIOS_PRODUCT_NAME`` - Product name  
* ``CONFIG_SMBIOS_VERSION`` - Product version
* ``CONFIG_SMBIOS_SERIAL_NUMBER`` - Serial number
* ``CONFIG_SMBIOS_SKU_NUMBER`` - Stock Keeping Unit number
* ``CONFIG_SMBIOS_FAMILY`` - Product family

These can also be overridden at runtime using environment variables with
the same names (in lowercase).

SMBIOS Table Types
------------------

U-Boot generates several standard SMBIOS table types:

Type 0: BIOS Information
  Contains BIOS vendor, version, release date, and characteristics

Type 1: System Information  
  Contains system manufacturer, product name, version, serial number, UUID, 
  SKU number, and family

Type 2: Baseboard Information
  Contains motherboard/baseboard manufacturer, product name, version, and 
  serial number

Type 3: System Enclosure
  Contains chassis information including type, manufacturer, version, and
  serial number

Type 4: Processor Information
  Contains CPU information including family, manufacturer, version, and
  characteristics

Type 16: Physical Memory Array
  Describes the physical memory configuration

Type 17: Memory Device
  Describes individual memory modules

Type 19: Memory Array Mapped Address
  Maps physical memory arrays to system addresses

Type 32: System Boot Information
  Contains boot status information

Using SMBIOS Tables
-------------------

Generated SMBIOS tables are typically placed in memory where the operating
system can find them. The location varies by architecture:

* **x86**: Tables placed in conventional memory below 1MB
* **ARM/AArch64**: Tables passed via device tree or ACPI

The smbios command
------------------

U-Boot provides the ``smbios`` command to display SMBIOS information::

  smbios [type]

Without arguments, it displays a summary of all SMBIOS tables. With a numeric
type argument, it displays detailed information for that specific table type.

Examples::

  smbios           # Show all tables summary
  smbios 1         # Show System Information (Type 1) details
  smbios 4         # Show Processor Information (Type 4) details

References
----------

* `DMTF SMBIOS Specification <https://www.dmtf.org/standards/smbios>`_
* `Microsoft System and Enclosure Chassis Types <https://docs.microsoft.com/en-us/windows-hardware/drivers/bringup/smbios>`_
* `Computer Hardware ID (CHID) Specification <https://docs.microsoft.com/en-us/windows-hardware/drivers/install/specifying-hardware-ids-for-a-computer>`_
