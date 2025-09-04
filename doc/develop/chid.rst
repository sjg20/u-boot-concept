.. SPDX-License-Identifier: GPL-2.0+

Computer Hardware IDs (CHID)
=============================

Overview
--------

Computer Hardware IDs (CHIDs) are Microsoft-defined identifiers used by
Windows Update and Linux fwupd for hardware identification and firmware
update targeting. CHIDs provide a standardized way to uniquely identify
hardware platforms using SMBIOS information.

U-Boot implements CHID generation following Microsoft's specification to
enable compatibility with firmware update ecosystems that rely on these
identifiers.

CHID Generation Algorithm
-------------------------

CHIDs are generated using the following process:

1. **Field Selection**: Based on the variant (0-14), select which SMBIOS
   fields to include
2. **String Building**: Concatenate selected fields with '&' separators
3. **String Trimming**: Remove leading/trailing whitespace from each field
4. **UTF-16LE Conversion**: Convert the concatenated string to UTF-16LE
   encoding
5. **UUID Generation**: Generate a UUID v5 using Microsoft's namespace
   UUID and the UTF-16LE data

CHID Variants
-------------

Microsoft defines 15 CHID variants (HardwareID-00 through HardwareID-14),
ordered from most specific to least specific:

=================== ====================================================
Variant             Fields Used
=================== ====================================================
HardwareID-00       Manufacturer + Family + ProductName + ProductSku +
                    BiosVendor + BiosVersion + BiosMajorRelease +
                    BiosMinorRelease
HardwareID-01       Manufacturer + Family + ProductName + BiosVendor +
                    BiosVersion + BiosMajorRelease + BiosMinorRelease
HardwareID-02       Manufacturer + ProductName + BiosVendor +
                    BiosVersion + BiosMajorRelease + BiosMinorRelease
HardwareID-03       Manufacturer + Family + ProductName + ProductSku +
                    BaseboardManufacturer + BaseboardProduct
HardwareID-04       Manufacturer + Family + ProductName + ProductSku
HardwareID-05       Manufacturer + Family + ProductName
HardwareID-06       Manufacturer + ProductSku + BaseboardManufacturer +
                    BaseboardProduct
HardwareID-07       Manufacturer + ProductSku
HardwareID-08       Manufacturer + ProductName + BaseboardManufacturer +
                    BaseboardProduct
HardwareID-09       Manufacturer + ProductName
HardwareID-10       Manufacturer + Family + BaseboardManufacturer +
                    BaseboardProduct
HardwareID-11       Manufacturer + Family
HardwareID-12       Manufacturer + EnclosureKind
HardwareID-13       Manufacturer + BaseboardManufacturer +
                    BaseboardProduct
HardwareID-14       Manufacturer
=================== ====================================================

SMBIOS Field Mapping
--------------------

CHIDs use the following SMBIOS table fields:

================= ================= ===========
CHID Field        SMBIOS Table      Field
================= ================= ===========
Manufacturer      Type 1 (System)   Manufacturer
Family            Type 1 (System)   Family
ProductName       Type 1 (System)   Product Name
ProductSku        Type 1 (System)   SKU Number
BaseboardManuf    Type 2 (Board)     Manufacturer
BaseboardProduct  Type 2 (Board)     Product Name
BiosVendor        Type 0 (BIOS)      Vendor
BiosVersion       Type 0 (BIOS)      Version
BiosMajorRelease  Type 0 (BIOS)      Major Release
BiosMinorRelease  Type 0 (BIOS)      Minor Release
EnclosureKind     Type 3 (Chassis)   Type
================= ================= ===========

Technical Details
-----------------

The CHID generation algorithm follows Microsoft's exact specification to ensure
compatibility with Windows Update and fwupd. The process begins by concatenating
the selected SMBIOS fields with '&' separators, creating strings like
``Manufacturer&Family&ProductName&ProductSku``. These strings are then converted
to UTF-16LE encoding to properly handle international characters.

UUID generation uses the SHA-1 based UUID v5 algorithm with Microsoft's specific
namespace UUID ``70ffd812-4c7f-4c7d-0000-000000000000``. The implementation
generates big-endian UUIDs to match Microsoft's ComputerHardwareIds.exe output
exactly, ensuring byte-for-byte compatibility with the reference implementation.

API Reference
-------------

Core Functions
~~~~~~~~~~~~~~

.. c:function:: int chid_from_smbios(struct chid_data *chid)

   Extract CHID-relevant data from SMBIOS tables.

   :param chid: Pointer to structure to fill with SMBIOS data
   :returns: 0 on success, negative error code on failure

.. c:function:: int chid_generate(int variant, const struct chid_data *data, u8 chid[16])

   Generate a specific CHID variant.

   :param variant: CHID variant number (0-14)
   :param data: SMBIOS data structure
   :param chid: Output buffer for 16-byte CHID
   :returns: 0 on success, negative error code on failure

Utility Functions
~~~~~~~~~~~~~~~~~

.. c:function:: const char *chid_get_field_name(enum chid_field_t field)

   Get display name for a CHID field.

   :param field: CHID field identifier
   :returns: Human-readable field name

.. c:function:: u32 chid_get_variant_fields(int variant)

   Get bitmask of fields used by a CHID variant.

   :param variant: CHID variant number (0-14)
   :returns: Bitmask of fields (BIT(CHID_xxx) values)

.. c:function:: const char *chid_get_variant_name(int variant)

   Get name of a CHID variant.

   :param variant: CHID variant number (0-14)
   :returns: Variant name (e.g., "HardwareID-00")

Data Structures
~~~~~~~~~~~~~~~

**struct chid_data**

   Contains SMBIOS field values for CHID generation::

       struct chid_data {
           const char *manuf;
           const char *family;
           const char *product_name;
           const char *product_sku;
           const char *board_manuf;
           const char *board_product;
           const char *bios_vendor;
           const char *bios_version;
           u8 bios_major;
           u8 bios_minor;
           u8 enclosure_type;
       };

**enum chid_field_t**

   CHID field identifiers::

       enum chid_field_t {
           CHID_MANUF,
           CHID_FAMILY,
           CHID_PRODUCT_NAME,
           CHID_PRODUCT_SKU,
           CHID_BOARD_MANUF,
           CHID_BOARD_PRODUCT,
           CHID_BIOS_VENDOR,
           CHID_BIOS_VERSION,
           CHID_BIOS_MAJOR,
           CHID_BIOS_MINOR,
           CHID_ENCLOSURE_TYPE,
           CHID_COUNT,
       };

Command Interface
-----------------

See :doc:`/usage/cmd/chid`.

Devicetree Generation Script
-----------------------------

The ``scripts/hwids_to_dtsi.py`` script converts HWIDS text files containing
computer information and hardware IDs to devicetree source (.dtsi) files. This
enables embedding CHID data directly in devicetree for platforms that need it.

Usage
~~~~~

**Single File Mode**::

   python scripts/hwids_to_dtsi.py board/efi/hwids/device.txt -o device.dtsi

**Multi-Board Mode**::

   python scripts/hwids_to_dtsi.py -m board/efi/hwids/compatible.hwidmap -o hwids.dtsi

The script processes HWIDS files generated by Microsoft's ComputerHardwareIds.exe
utility and creates devicetree nodes containing:

* SMBIOS computer information as devicetree properties
* Hardware ID arrays as binary CHID data
* Compatible strings for device matching

Input Format
~~~~~~~~~~~~

HWIDS files contain computer information and hardware ID sections::

   Computer Information
   --------------------
   BiosVendor: ACME Corp
   BiosVersion: V1.0
   Manufacturer: ACME
   ProductName: Test Device
   ...

   Hardware IDs
   ------------
   {12345678-1234-5678-9abc-123456789abc} <- Field description
   {87654321-4321-8765-cba9-987654321cba} <- Field description
   ...

The script parses both sections and generates corresponding devicetree properties
and CHID arrays.

Output Format
~~~~~~~~~~~~~

The output consists of a node for each device. Within that node the compatible
string is provided, along the SMBIOS information to match against. Then there is
a subnode for each CHID variant, containing the variant number, a bitmask
indicating which fields are included in that variant and finally the CHID
itself (16 bytes).

For example::

   // SPDX-License-Identifier: GPL-2.0+

   // Computer Hardware IDs for multiple boards
   // Generated from board/efi/hwids/

   / {
      chid: chid {};
   };

   &chid {
      device-name {
         compatible = "vendor,device";

         // SMBIOS Computer Information
         manufacturer = "ACME";
         product-name = "Test Device";
         bios-vendor = "ACME Corp";

         // Hardware IDs (CHIDs)
         hardware-id-00 {
            variant = <0>;
            fields = <0x3cf>;
            chid = [12 34 56 78 12 34 56 78 9a bc 12 34 56 78 9a bc];
         };
      };
   };

**Devicetree Properties**

The generated devicetree contains the following properties:

========================= ========== ===========================================
Property                  Type       Purpose
========================= ========== ===========================================
compatible                string     Device identification for matching
manufacturer              string     SMBIOS System Manufacturer
family                    string     SMBIOS System Family
product-name              string     SMBIOS System Product Name
product-sku               string     SMBIOS System SKU Number
baseboard-manufacturer    string     SMBIOS Board Manufacturer
baseboard-product         string     SMBIOS Board Product Name
bios-vendor               string     SMBIOS BIOS Vendor
bios-version              string     SMBIOS BIOS Version
bios-major-release        u32        SMBIOS BIOS Major Release
bios-minor-release        u32        SMBIOS BIOS Minor Release
firmware-major-release    u32        SMBIOS Firmware Major Release (EFI only)
firmware-minor-release    u32        SMBIOS Firmware Minor Release (EFI only)
enclosure-kind            u32        SMBIOS Chassis Type (hex format)
variant                   u32        CHID variant number (0-14). Omitted if
                                     there is no variant.
fields                    u32        Bitmask of fields used in CHID generation
chid                      byte-array 16-byte CHID UUID in binary format
========================= ========== ===========================================

Compatible Mapping
~~~~~~~~~~~~~~~~~~

Multi-board mode uses a ``compatible.hwidmap`` file to map device names to
compatible strings::

   # Device mapping file
   device1: vendor,device1
   device2: vendor,device2
   special-board: none        # Skip this board

Lines starting with ``#`` are comments. Use ``none`` as the compatible string
to skip processing a particular board.

Testing
-------

Tests are provided in:

* ``test/lib/chid.c`` - Library function tests
* ``test/cmd/chid.c`` - Command interface tests
* ``test/scripts/test_hwids_to_dtsi.py`` - Script functionality tests

Tests validate against real Microsoft ComputerHardwareIds.exe output
to ensure exact compatibility. The script tests verify HWIDS file parsing,
devicetree generation, and error handling.

References
----------

* :doc:`smbios` - SMBIOS table information used by CHIDs
* `fwupd Hardware IDs <https://github.com/fwupd/fwupd/blob/main/docs/hwids.md>`_
* `Microsoft Hardware ID Specification <https://docs.microsoft.com/en-us/windows-hardware/drivers/install/specifying-hardware-ids-for-a-computer>`_
* `Reverse Engineering Blog Post <https://blogs.gnome.org/hughsie/2017/04/25/reverse-engineering-computerhardwareids-exe-with-winedbg/>`_
