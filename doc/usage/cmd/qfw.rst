.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: qfw (command)

qfw command
===========

Synopsis
--------

::

    qfw dump
    qfw list
    qfw cpus
    qfw load [kernel_addr [initrd_addr]]
    qfw table

Description
-----------

The *qfw* command is used to retrieve information from the QEMU firmware.

The *qfw list* sub-command displays the QEMU firmware files.

The *qfw dump* sub-command displays the QEMU configuration entries.

The *qfw cpus* sub-command displays the available CPUs.

The *qfw load* command is used to load a kernel and an initial RAM disk.

The *qfw table* command shows the instructions U-Boot uses to construct the
ACPI tables. This is not as simple as a single blob of data, since some entries
must be updated to take account of the address to which the tables are copied.

kernel_addr
    address to which the file specified by the -kernel parameter of QEMU shall
    be loaded. Defaults to environment variable *loadaddr* and further to
    the value of *CONFIG_SYS_LOAD_ADDR*.

initrd_addr
    address to which the file specified by the -initrd parameter of QEMU shall
    be loaded. Defaults to environment variable *ramdiskaddr* and further to
    the value of *CFG_RAMDISK_ADDR*.

Examples
--------

QEMU firmware files are listed via the *qfw list* command:

::

    => qfw list
    Addr     Size Sel Name
    -------- -------- --- ------------
           0        0  20 bios-geometry
           0        0  21 bootorder
    1fc6c000       14  22 etc/acpi/rsdp
    1fc6c040    20000  23 etc/acpi/tables
           0        4  24 etc/boot-fail-wait
           0       28  25 etc/e820
           0        8  26 etc/msr_feature_control
           0       18  27 etc/smbios/smbios-anchor
           0      151  28 etc/smbios/smbios-tables
           0        6  29 etc/system-states
           0     1000  2a etc/table-loader
           0        0  2b etc/tpm/log
           0     2400  2c genroms/kvmvapic.bin

Where an address is shown, it indicates where the data is available for
inspection, e.g. using the :doc:`md`.

This shows the *qfw dump* command on an x86 target:

::
    signature   = QEMU
    id          = 0x00000003
    uuid        = dc8275a0-59c6-11f0-b383-0f1ea10dd2f7
    ram_size    = 0x0000000020000000
    nographic   = 0x00000000
    nb cpus     = 0x00000004
    machine id  = 0x00000000
    kernel addr = 0x00100000
    kernel size = 0x00e9e988
    kernel cmdl = 0x00000000
    initrd addr = 0x1b446000
    initrd size = 0x04b91d57
    boot device = 0x00000000
    numa        = 0x00000000
    boot menu   = 0x00000000
    max cpus    = 0x00000004
    kernel entry= 0x00000000
    cmdline addr= 0x00020000
    cmdline size= 0x0000000f
    cmdline data= root=/dev/sda1
    setup addr  = 0x00010000
    setup size  = 0x00005000
    file dir le = 0x0000000d

The available CPUs can be shown via the *qfw cpus* command:

::

    => qfw cpu
    2 cpu(s) online

The *-kernel* and *-initrd* parameters allow to specify a kernel and an
initial RAM disk for QEMU:

.. code-block:: bash

   $ qemu-system-x86_64 -machine pc-i440fx-2.5 -bios u-boot.rom -m 1G \
       -nographic -kernel vmlinuz -initrd initrd

Now the kernel and the initial RAM disk can be loaded to the U-Boot memory via
the *qfw load* command and booted thereafter.

::

    => qfw load ${kernel_addr_r} ${ramdisk_addr_r}
    loading kernel to address 0000000001000000 size 5048f0 initrd 0000000004000000 size 3c94891
    => zboot 1000000 5048f0 4000000 3c94891
    Valid Boot Flag
    Magic signature found
    Linux kernel version 4.19.0-14-amd64 (debian-kernel@lists.debian.org) #1 SMP Debian 4.19.171-2 (2021-01-30)
    Building boot_params at 0x00090000
    Loading bzImage at address 100000 (5260160 bytes)


This shows the *qfw table* command on an x86 target:

::

    => qfw table
      0 alloc: align 10 zone fseg name 'etc/acpi/rsdp'
      1 alloc: align 40 zone high name 'etc/acpi/tables'
      2 add-chksum offset 49 start 40 length 1ab4 name 'etc/acpi/tables'
      3 add-ptr offset 1b18 size 4 dest 'etc/acpi/tables' src 'etc/acpi/tables'
      4 add-ptr offset 1b1c size 4 dest 'etc/acpi/tables' src 'etc/acpi/tables'
      5 add-chksum offset 1afd start 1af4 length 74 name 'etc/acpi/tables'
      6 add-chksum offset 1b71 start 1b68 length 78 name 'etc/acpi/tables'
      7 add-chksum offset 1be9 start 1be0 length 38 name 'etc/acpi/tables'
      8 add-chksum offset 1c21 start 1c18 length 28 name 'etc/acpi/tables'
      9 add-ptr offset 1c64 size 4 dest 'etc/acpi/tables' src 'etc/acpi/tables'
     10 add-ptr offset 1c68 size 4 dest 'etc/acpi/tables' src 'etc/acpi/tables'
     11 add-ptr offset 1c6c size 4 dest 'etc/acpi/tables' src 'etc/acpi/tables'
     12 add-ptr offset 1c70 size 4 dest 'etc/acpi/tables' src 'etc/acpi/tables'
     13 add-chksum offset 1c49 start 1c40 length 34 name 'etc/acpi/tables'
     14 add-ptr offset 10 size 4 dest 'etc/acpi/rsdp' src 'etc/acpi/tables'
     15 add-chksum offset 8 start 0 length 14 name 'etc/acpi/rsdp'

Configuration
-------------

The qfw command is only available if CONFIG_CMD_QFW=y.
