.. SPDX-License-Identifier: GPL-2.0+
.. Copyright 2024 Google LLC
.. Written by Simon Glass <sjg@chromium.org>

.. index::
   single: efidebug (command)

efidebug command
================

Synopsis
--------

::

    efidebug boot add -b <bootid> <label> <interface> <devnum>[:<part>] <file>
    efidebug boot rm <bootid> [<bootid> ...]
    efidebug boot dump
    efidebug boot next <bootid>
    efidebug boot order [<bootid> ...]
    efidebug log
    efidebug media
    efidebug memmap [-s]

Description
-----------

The *efidebug* command provides access some EFI features, including boot
management, memory mapping, and system information.

efidebug boot
~~~~~~~~~~~~~

The boot subcommands manage UEFI boot options (BootXXXX variables) and boot
order.

**efidebug boot add**
    Create or modify a UEFI boot option. The basic syntax is:
    ``efidebug boot add -b <bootid> <label> <interface> <devnum>[:<part>] <file>``

    Where:

    - ``<bootid>`` is a hexadecimal boot option ID (e.g., 0001, 000A)
    - ``<label>`` is a descriptive name for the boot option
    - ``<interface>`` is the storage interface (e.g., mmc, usb, virtio)
    - ``<devnum>[:<part>]`` specifies device and optionally partition number
    - ``<file>`` is the path to the EFI executable

    Additional options include ``-i`` for initrd, ``-s`` for optional data,
    and ``-d`` for device tree files.

**efidebug boot rm**
    Remove one or more UEFI boot options by their boot IDs.

**efidebug boot dump**
    List all defined UEFI boot options with their details including boot ID,
    label, and file path.

**efidebug boot next**
    Set the BootNext variable to specify which boot option should be used for
    the next boot only.

**efidebug boot order**
    Show the current boot order when called without arguments, or sets a new
    boot order when given a list of boot IDs.

efidebug log
~~~~~~~~~~~~

This shows a log of EFI boot-services calls which have been handled since U-Boot
started. This can be useful to see what the app is doing, or even what U-Boot
itself has called.

efidebug media
~~~~~~~~~~~~~~

This shows a list of all EFI media devices, their likely U-Boot uclass, and
their corresponding device paths. Each EFI media device represents a block
device that was discovered through EFI boot services, such as hard drives, USB
storage, or other bootable media. The U-Boot Class column shows which U-Boot
driver subsystem would likely handle the device (e.g., "ahci" for SATA drives,
"usb" for USB storage). The device path shows the EFI device path for each
device, which can be useful for debugging boot issues or understanding the
system topology.

efidebug memmap
~~~~~~~~~~~~~~~

This shows the UEFI memory map, which displays all memory regions and their
types as known to the EFI loader subsystem. This includes information about
memory allocation, reserved regions, and available memory.

The command supports an optional '-s' flag to sort the memory map entries by
address, making it easier to visualize the memory layout in ascending order.

Example
-------

This shows managing UEFI boot options::

    => efidebug boot add -b 1 "Ubuntu" mmc 0:2 /EFI/ubuntu/grubx64.efi
    => efidebug boot add -b 2 "Windows" mmc 0:1 /EFI/Microsoft/Boot/bootmgfw.efi
    => efidebug boot dump
    Boot0001:
    attributes: A-- (0x00000001)
    label: Ubuntu
    file_path: /VenHw(e61d73b9-a384-4acc-aeab-82e828f3628b)/SD(0)/
               HD(2,GPT,dfae5a37-e8d5-4a2e-aab6-6b6e8c8cb8a3,0x100800,
               0x5dc1800)/\EFI\ubuntu\grubx64.efi
    data: 
    Boot0002:
    attributes: A-- (0x00000001)
    label: Windows
    file_path: /VenHw(e61d73b9-a384-4acc-aeab-82e828f3628b)/SD(0)/
               HD(1,GPT,c8a9e8e7-2d2a-4b8e-9b4f-7b7b3b7b7b7b,0x800,
               0x100000)/\EFI\Microsoft\Boot\bootmgfw.efi
    data:
    => efidebug boot order 1 2
    => efidebug boot order
    BootOrder: 0001 0002
    => efidebug boot next 2
    => efidebug boot rm 1

This shows checking the EFI media devices::

    => efidebug media
    Device               Media type       Device Path
    -------------------  ---------------  -----------
    efi_media_1          ahci             PciRoot(0x0)/Pci(0x3,0x0)/
                                          Sata(0x0,0xFFFF,0x0)
    efi_media_2          pci              PciRoot(0x0)/Pci(0x5,0x0)

This shows checking the UEFI memory map, first unsorted and then sorted by
address::

    => efidebug mem
    Type             Start            End              Attributes
    ================ ================ ================ ==========
    CONVENTIONAL     0000000040000000-0000000044000000 WB
    BOOT DATA        0000000044000000-0000000044020000 WB
    CONVENTIONAL     0000000044020000-00000000475ee000 WB
    BOOT DATA        00000000475ee000-0000000047610000 WB
    BOOT CODE        0000000047610000-0000000047647000 WB
    BOOT DATA        0000000047647000-0000000047ef2000 WB
    BOOT CODE        0000000047ef2000-0000000047ef6000 WB
    BOOT DATA        0000000047ef6000-0000000047ff7000 WB
    BOOT CODE        0000000047ff7000-0000000047ffa000 WB
    BOOT DATA        0000000047ffa000-0000000048000000 WB
    CONVENTIONAL     0000000048000000-00000000e0000000 WB
    LOADER DATA      00000000e0000000-0000000100000000 WB
    CONVENTIONAL     0000000100000000-000000013c278000 WB
    LOADER DATA      000000013c278000-000000013c27c000 WB
    LOADER CODE      000000013c27c000-000000013c3e0000 WB
    ACPI RECLAIM MEM 000000013c3e0000-000000013c3f0000 WB
    RUNTIME CODE     000000013c3f0000-000000013c470000 WB|RT
    RUNTIME DATA     000000013c470000-000000013c630000 WB|RT
    RUNTIME CODE     000000013c630000-000000013c730000 WB|RT
    CONVENTIONAL     000000013c730000-000000013dc2a000 WB
    BOOT DATA        000000013dc2a000-000000013e9f1000 WB
    CONVENTIONAL     000000013e9f1000-000000013e9fe000 WB
    BOOT DATA        000000013e9fe000-000000013ea1c000 WB
    CONVENTIONAL     000000013ea1c000-000000013ea1e000 WB
    BOOT DATA        000000013ea1e000-000000013ea47000 WB
    CONVENTIONAL     000000013ea47000-000000013ea48000 WB
    BOOT DATA        000000013ea48000-000000013f624000 WB
    CONVENTIONAL     000000013f624000-000000013f731000 WB
    BOOT CODE        000000013f731000-000000013fc00000 WB
    RUNTIME CODE     000000013fc00000-000000013fd90000 WB|RT
    RUNTIME DATA     000000013fd90000-000000013ffe0000 WB|RT
    CONVENTIONAL     000000013ffe0000-000000013ffff000 WB
    BOOT DATA        000000013ffff000-0000000140000000 WB
    IO               0000000004000000-0000000008000000 UC|RT
    IO               0000000009010000-0000000009011000 UC|RT
    => efidebug mem -s
    Type             Start            End              Attributes
    ================ ================ ================ ==========
    IO               0000000004000000-0000000008000000 UC|RT
    IO               0000000009010000-0000000009011000 UC|RT
    CONVENTIONAL     0000000040000000-0000000044000000 WB
    BOOT DATA        0000000044000000-0000000044020000 WB
    CONVENTIONAL     0000000044020000-00000000475ee000 WB
    BOOT DATA        00000000475ee000-0000000047610000 WB
    BOOT CODE        0000000047610000-0000000047647000 WB
    BOOT DATA        0000000047647000-0000000047ef2000 WB
    BOOT CODE        0000000047ef2000-0000000047ef6000 WB
    BOOT DATA        0000000047ef6000-0000000047ff7000 WB
    BOOT CODE        0000000047ff7000-0000000047ffa000 WB
    BOOT DATA        0000000047ffa000-0000000048000000 WB
    CONVENTIONAL     0000000048000000-00000000e0000000 WB
    LOADER DATA      00000000e0000000-0000000100000000 WB
    CONVENTIONAL     0000000100000000-000000013c278000 WB
    LOADER DATA      000000013c278000-000000013c27c000 WB
    LOADER CODE      000000013c27c000-000000013c3e0000 WB
    ACPI RECLAIM MEM 000000013c3e0000-000000013c3f0000 WB
    RUNTIME CODE     000000013c3f0000-000000013c470000 WB|RT
    RUNTIME DATA     000000013c470000-000000013c630000 WB|RT
    RUNTIME CODE     000000013c630000-000000013c730000 WB|RT
    CONVENTIONAL     000000013c730000-000000013dc2a000 WB
    BOOT DATA        000000013dc2a000-000000013e9f1000 WB
    CONVENTIONAL     000000013e9f1000-000000013e9fe000 WB
    BOOT DATA        000000013e9fe000-000000013ea1c000 WB
    CONVENTIONAL     000000013ea1c000-000000013ea1e000 WB
    BOOT DATA        000000013ea1e000-000000013ea47000 WB
    CONVENTIONAL     000000013ea47000-000000013ea48000 WB
    BOOT DATA        000000013ea48000-000000013f624000 WB
    CONVENTIONAL     000000013f624000-000000013f731000 WB
    BOOT CODE        000000013f731000-000000013fc00000 WB
    RUNTIME CODE     000000013fc00000-000000013fd90000 WB|RT
    RUNTIME DATA     000000013fd90000-000000013ffe0000 WB|RT
    CONVENTIONAL     000000013ffe0000-000000013ffff000 WB
    BOOT DATA        000000013ffff000-0000000140000000 WB
    =>

This shows checking the log, then using 'efidebug tables' to fully set up the
EFI-loader subsystem, then checking the log again::

    => efidebug log
    EFI log (size 158)
    0   alloc_pool bt-data size 33/51 buf 7fffd8448ad0 *buf 7c20010 ret OK
    1  alloc_pages any-pages bt-data pgs 1 mem 7fffd8448a80 *mem 7c20000 ret OK
    2   alloc_pool bt-data size 60/96 buf 7fffd8448ac0 *buf 7c1f010 ret OK
    3  alloc_pages any-pages bt-data pgs 1 mem 7fffd8448a60 *mem 7c1f000 ret OK
    4   alloc_pool bt-data size 60/96 buf 7fffd8448ac0 *buf 7c1e010 ret OK
    5  alloc_pages any-pages bt-data pgs 1 mem 7fffd8448a60 *mem 7c1e000 ret OK
    6 records
    => efidebug tables
    efi_var_to_file() Cannot persist EFI variables without system partition
    0000000017bfc010  36122546-f7ef-4c8f-bd9b-eb8525b50c0b  EFI Conformance Profiles Table
    0000000017bd4010  b122a263-3661-4f68-9929-78f8b0d62180  EFI System Resource Table
    0000000017bd8010  1e2ed096-30e2-4254-bd89-863bbef82325  TCG2 Final Events Table
    0000000017bd6010  eb66918a-7eef-402a-842e-931d21c38ae9  Runtime properties
    0000000008c49000  8868e871-e4f1-11d3-bc22-0080c73c8881  ACPI table
    0000000018c5b000  f2fd1544-9794-4a2c-992e-e5bbcf20e394  SMBIOS3 table
    => efidebug log
    EFI log (size a20)
    0   alloc_pool bt-data size 33/51 buf 7fffd8448ad0 *buf 7c20010 ret OK
    1  alloc_pages any-pages bt-data pgs 1 mem 7fffd8448a80 *mem 7c20000 ret OK
    2   alloc_pool bt-data size 60/96 buf 7fffd8448ac0 *buf 7c1f010 ret OK
    3  alloc_pages any-pages bt-data pgs 1 mem 7fffd8448a60 *mem 7c1f000 ret OK
    4   alloc_pool bt-data size 60/96 buf 7fffd8448ac0 *buf 7c1e010 ret OK
    5  alloc_pages any-pages bt-data pgs 1 mem 7fffd8448a60 *mem 7c1e000 ret OK
    6  alloc_pages any-pages rt-data pgs 20/32 mem 7fffd8448838 *mem 7bfe000 ret OK
    7   alloc_pool rt-data size 60/96 buf 7fffd84487e0 *buf 7bfd010 ret OK
    8  alloc_pages any-pages rt-data pgs 1 mem 7fffd8448780 *mem 7bfd000 ret OK
    9   alloc_pool rt-data size 180/384 buf 56f190ffd890 *buf 7bfc010 ret OK
    10  alloc_pages any-pages rt-data pgs 1 mem 7fffd8448800 *mem 7bfc000 ret OK
    11   alloc_pool bt-data size 4 buf 7fffd8448840 *buf 7bfb010 ret OK
    12  alloc_pages any-pages bt-data pgs 1 mem 7fffd84487f0 *mem 7bfb000 ret OK
    13   alloc_pool bt-data size 10/16 buf 7fffd8448728 *buf 7bfa010 ret OK
    14  alloc_pages any-pages bt-data pgs 1 mem 7fffd84486d0 *mem 7bfa000 ret OK
    15   alloc_pool bt-data size 60/96 buf 7fffd84487e0 *buf 7bf9010 ret OK
    16  alloc_pages any-pages bt-data pgs 1 mem 7fffd8448780 *mem 7bf9000 ret OK
    17   alloc_pool bt-data size 10000/65536 buf 56f19100fae0 *buf 7be8010 ret OK
    18  alloc_pages any-pages bt-data pgs 11/17 mem 7fffd84487d0 *mem 7be8000 ret OK
    19   alloc_pool acpi-nvs size 10000/65536 buf 56f19100fae8 *buf 7bd7010 ret OK
    20  alloc_pages any-pages acpi-nvs pgs 11/17 mem 7fffd84487d0 *mem 7bd7000 ret OK
    21   alloc_pool bt-data size 60/96 buf 7fffd84487d0 *buf 7bd6010 ret OK
    22  alloc_pages any-pages bt-data pgs 1 mem 7fffd8448770 *mem 7bd6000 ret OK
    23   alloc_pool rt-data size 8 buf 7fffd8448818 *buf 7bd5010 ret OK
    24  alloc_pages any-pages rt-data pgs 1 mem 7fffd84487c0 *mem 7bd5000 ret OK
    25   alloc_pool bt-data size 8 buf 7fffd8448360 *buf 7bd4010 ret OK
    26  alloc_pages any-pages bt-data pgs 1 mem 7fffd8448160 *mem 7bd4000 ret OK
    27   alloc_pool bt-data size f0/240 buf 7fffd8448378 *buf 7bd3010 ret OK
    28  alloc_pages any-pages bt-data pgs 1 mem 7fffd84482d0 *mem 7bd3000 ret OK
    29    free_pool buf 7bd3010 ret OK
    30   free_pages mem 7bd3000 pag 1 ret OK
    31   alloc_pool bt-data size 60/96 buf 7fffd84482d8 *buf 7bd3010 ret OK
    32  alloc_pages any-pages bt-data pgs 1 mem 7fffd8448280 *mem 7bd3000 ret OK
    33    free_pool buf 7bfa010 ret OK
    34   free_pages mem 7bfa000 pag 1 ret OK
    35   alloc_pool bt-data size f0/240 buf 7fffd8448380 *buf 7bfa010 ret OK
    36  alloc_pages any-pages bt-data pgs 1 mem 7fffd84482d0 *mem 7bfa000 ret OK
    37    free_pool buf 7bfa010 ret OK
    38   free_pages mem 7bfa000 pag 1 ret OK
    39    free_pool buf 7bd4010 ret OK
    40   free_pages mem 7bd4000 pag 1 ret OK
    41   alloc_pool bt-data size 61/97 buf 7fffd8448810 *buf 7bfa010 ret OK
    42  alloc_pages any-pages bt-data pgs 1 mem 7fffd84487c0 *mem 7bfa000 ret OK
    43   alloc_pool bt-data size 60/96 buf 7fffd8448800 *buf 7bd4010 ret OK
    44  alloc_pages any-pages bt-data pgs 1 mem 7fffd84487a0 *mem 7bd4000 ret OK
    45   alloc_pool bt-data size 60/96 buf 7fffd8448800 *buf 7bd2010 ret OK
    46  alloc_pages any-pages bt-data pgs 1 mem 7fffd84487a0 *mem 7bd2000 ret OK
    47   alloc_pool bt-data size 60/96 buf 7fffd8448810 *buf 7bd1010 ret OK
    48  alloc_pages any-pages bt-data pgs 1 mem 7fffd84487b0 *mem 7bd1000 ret OK
    49 records
    =>
