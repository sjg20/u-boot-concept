.. SPDX-License-Identifier: GPL-2.0+

fru command
===========

Synopsis
--------

::

    fru capture <addr>
    fru display
    fru generate -b <addr> <manufacturer> <board name> <serial number> <part number> <file id> [<custom> ...]
    fru generate -p <addr> <manufacturer> <product name> <part number> <version number> <serial number> <asset number> <file id> [<custom> ...]

Description
-----------

The *fru* commands is used to generate, capture and display FRU (Field
Replaceable Unit) information data.

Capture
~~~~~~~

The *fru capture* command parses and captures FRU configuration table at
a specified address.

    addr
        memory address which FRU configuration table is stored.

Display
~~~~~~~

The *fru display* command displays FRU information that is parsed using
fru capture command.

Generate
~~~~~~~~

The *fru generate* command generates a FRU configuration table which has Board
or Product Info Area using the given field parameters.

    -b
        generate FRU which has board info area.

        addr
            memory address which FRU configuration table will be stored.

        manufacturer
            board manufacturer string.

        board name
            board product name string.

        serial number
            board serial number string.

        serial number
            board serial number string.

        part number
            board part number string.

        file id
            FRU File ID string. The FRU File version field is a pre-defined
            field provided as a manufacturing aid for verifying the file that
            was used during manufacture or field update to load the FRU
            information. The content is manufacturer-specific.

        custom
            additional custom board info area fields, if any.

    -p
        generate FRU which has product info area.

        addr
            memory address which FRU configuration table will be stored.

        manufacturer
            product manufacturer string.

        board name
            product name string.

        part number
            product part/model number string.

        version number
            product version number string.

        serial number
            product serial number string.

        asset number
            asset tag.

        file id
            FRU File ID string. The FRU File version field is a pre-defined
            field provided as a manufacturing aid for verifying the file that
            was used during manufacture or field update to load the FRU
            information. The content is manufacturer-specific.

        custom
            additional custom product info area fields, if any.

Example
-------

::

    => fru generate -b 90000000 abc def ghi jkl mno prs tuv wxy
    => fru capture 90000000
    => fru display
    *****COMMON HEADER*****
    Version:1
    *** No Internal Area ***
    *** No Chassis Info Area ***
    Board Area Offset:8
    *** No Product Info Area ***
    *** No MultiRecord Area ***
    *****BOARD INFO*****
    Version:1
    Board Area Length:40
    Time in Minutes from 0:00hrs 1/1/96: 0
     Manufacturer Name: abc
     Product Name: def
     Serial Number: ghi
     Part Number: jkl
     File ID: mno
     Custom Type/Length: 0xc3
      00000000: 70 72 73                                         prs
     Custom Type/Length: 0xc3
      00000000: 74 75 76                                         tuv
     Custom Type/Length: 0xc3
      00000000: 77 78 79                                         wxy
    *****PRODUCT INFO*****
    Version:0
    Product Area Length:0
    *****MULTIRECORDS*****

Configuration
-------------

The fru command is only available if CONFIG_CMD_FRU=y.
