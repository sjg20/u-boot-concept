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

Providing Values
~~~~~~~~~~~~~~~~

SMBIOS field values can be provided through two mechanisms, in order of
precedence:

1. **Sysinfo Driver**: A sysinfo driver (``UCLASS_SYSINFO``) can provide dynamic
   values at runtime through predefined sysinfo IDs.

2. **Device Tree Properties**: Static values can be defined in device tree nodes
   corresponding to each SMBIOS table type.

Device Tree Configuration
^^^^^^^^^^^^^^^^^^^^^^^^^

SMBIOS values can be specified in device tree nodes, but only when a sysinfo
driver is present. The 'smbios' node must be a subnode of the sysinfo device
in the device tree.

The following smbios subnodes and properties are supported:

**BIOS Information** (``smbios/bios``):
  * ``version`` - BIOS version string

**System Information** (``smbios/system``):
  * ``manufacturer`` - System manufacturer
  * ``product`` - Product name
  * ``version`` - System version
  * ``serial`` - Serial number
  * ``sku`` - SKU number
  * ``family`` - Product family
  * ``wakeup-type`` - System wakeup type (numeric)

**Baseboard Information** (``smbios/baseboard``):
  * ``manufacturer`` - Baseboard manufacturer
  * ``product`` - Baseboard product name
  * ``version`` - Baseboard version
  * ``serial`` - Baseboard serial number
  * ``asset-tag`` - Asset tag number
  * ``feature-flags`` - Feature flags (numeric)
  * ``chassis-location`` - Location in chassis

**Chassis Information** (``smbios/chassis``):
  * ``manufacturer`` - Chassis manufacturer
  * ``version`` - Chassis version
  * ``serial`` - Chassis serial number
  * ``asset-tag`` - Chassis asset tag
  * ``sku`` - Chassis SKU number

**Processor Information** (``smbios/processor``):
  * ``manufacturer`` - CPU manufacturer
  * ``version`` - Processor version/model
  * ``socket-design`` - Socket designation

**Cache Information** (``smbios/cache``):
  * ``socket-design`` - Socket designation

Example device tree configuration::

  sysinfo {
      compatible = "sandbox,sysinfo-sandbox";

      smbios {
          system {
              manufacturer = "Example Corp";
              product = "Example Board";
              version = "1.0";
              serial = "123456789";
              sku = "EXAMPLE-SKU-001";
              family = "Example Family";
          };

          baseboard {
              manufacturer = "Example Corp";
              product = "Example Baseboard";
              version = "Rev A";
              serial = "BB123456789";
          };
      };
  };

The device tree values serve as defaults and will be overridden by any values
provided by a sysinfo driver.

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

U-Boot provides the ``smbios`` command to display SMBIOS information.

Example::

  smbios           # Show all tables


References
----------

* `DMTF SMBIOS Specification <https://www.dmtf.org/standards/smbios>`_
* `Microsoft System and Enclosure Chassis Types <https://docs.microsoft.com/en-us/windows-hardware/drivers/bringup/smbios>`_
* `Computer Hardware ID (CHID) Specification <https://docs.microsoft.com/en-us/windows-hardware/drivers/install/specifying-hardware-ids-for-a-computer>`_
