.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: virtio (command)

virtio command
==============

Synopsis
--------

::

    virtio scan
    virtio info
    virtio list
    virtio device
    virtio part
    virtio read <addr> <blk#> <cnt>
    virtio write <addr> <blk#> <cnt>

Description
-----------

The *virtio* command is used to control a virtio block-device. It also provides
some more general features.

dev
    Device number fo select
addr
    memory address
blk#
    start block offset
cnt
    block count

virtio scan
~~~~~~~~~~~

This initiates a scan of the paravirtualized devices provided by QEMU. For each
QEMU device which U-Boot supports, a new device is bound, allowing it to be used
within U-Boot.

You can use *dm tree* to see all the devices.

virtio info
~~~~~~~~~~~

This lists the available block devices (only), including the name, type and
capacity.

virtio list
~~~~~~~~~~~

This shows a list of QEMU devices along with U-Boot each attached to each. Note
that QEMU devices that U-Boot doesn't support will not be assigned a driver.

virtio device
~~~~~~~~~~~~~

This selects a particular numbered device as the current one to be used for
virtio operations.

virtio part
~~~~~~~~~~~

This displays a list of the available partition on the selected device, or the
current one if the device is omitted.

virtio read
~~~~~~~~~~~

This reads raw data to memory address from a virtio block-device. Reading starts
at block number <blk#> (numbered from 0) and extends for <cnt> blocks. The
block size is typically 512 but you can change it in the QEMU configuration.
For example, to set the block size to 4K:

::
   qemu-system-x86_64
   -drive file=disk.img,if=none,id=drive0,format=raw \
   -device virtio-blk-pci,drive=drive0,id=virtio_disk0,logical_block_size=4096

virtio write
~~~~~~~~~~~~

This writes raw data to the virtio device from the given memory address. Writing
starts at block number <blk#> (numbered from 0) and extends for <cnt> blocks.

Examples
--------

For most of these examples, the following QEMU invocation was used with the
qemu-x86_64 build::

    qemu-system-x86_64 -bios u-boot.rom -nographic
       -drive if=virtio,file=root.img,format=raw,id=hd0
       -drive if=virtio,file=README,format=raw,id=hd1


This example shows scanning for devices and then listing them. You can see the
virtio device `virtio-pci.l#0` has a `virtio-blk#0` child. The 'No filesystem'
warning indicates that device 1 does not have a valid filesystem.

::

    => virtio scan
    => virtio info
    Device 0: 1af4 VirtIO Block Device
                Type: Hard Disk
                Capacity: 15360.0 MB = 15.0 GB (31457280 x 512)
    No filesystem (err -19)
    Device 1: 1af4 VirtIO Block Device
                Type: Hard Disk
                Capacity: 0.0 MB = 0.0 GB (171 x 512)
    => dm tree
    Class     Seq    Probed  Driver                Name
    -----------------------------------------------------------
    root          0  [ + ]   root_driver           root_driver
    qfw           0  [ + ]   qfw_pio               |-- qfw_pio
    bootdev       0  [   ]   qfw_bootdev           |   `-- qfw_pio.bootdev
    serial        0  [ + ]   ns16550_serial        |-- serial
    keyboard      0  [ + ]   i8042_kbd             |-- keyboard
    sysreset      0  [   ]   x86_reset             |-- reset
    rtc           0  [   ]   motorola_mc146818     |-- rtc
    timer         0  [ + ]   x86_tsc_timer         |-- tsc-timer
    sysinfo       0  [   ]   sysinfo_smbios        |-- smbios
    pci           0  [ + ]   pci_x86               |-- pci
    pch           0  [ + ]   intel-pch7            |   |-- pch@1,0
    pci_generi    0  [   ]   pci_generic_drv       |   |-- pci_0:0.0
    ide           0  [   ]   ide                   |   |-- ide
    pci_generi    1  [   ]   pci_generic_drv       |   |-- pci_0:1.3
    video         0  [ + ]   bochs_video           |   |-- bochs_video
    vidconsole    0  [ + ]   vidconsole_tt         |   |   `-- bochs_video.vidconsole_tt
    ethernet      0  [ + ]   eth_e1000             |   |-- e1000#0
    bootdev       1  [   ]   eth_bootdev           |   |   `-- e1000#0.bootdev
    virtio        0  [ + ]   virtio-pci.l          |   |-- virtio-pci.l#0
    blk           0  [ + ]   virtio-blk            |   |   |-- virtio-blk#0
    partition     0  [ + ]   blk_partition         |   |   |   |-- virtio-blk#0:1
    partition     1  [ + ]   blk_partition         |   |   |   `-- virtio-blk#0:2
    bootdev       2  [   ]   virtio_bootdev        |   |   `-- virtio-blk#0.bootdev
    virtio        1  [ + ]   virtio-pci.l          |   `-- virtio-pci.l#1
    blk           1  [   ]   virtio-blk            |       |-- virtio-blk#1
    bootdev       3  [   ]   virtio_bootdev        |       `-- virtio-blk#1.bootdev
    bootstd       0  [ + ]   bootstd_drv           |-- bootstd
    bootmeth      0  [   ]   bootmeth_extlinux     |   |-- extlinux
    bootmeth      1  [   ]   bootmeth_script       |   |-- script
    bootmeth      2  [   ]   bootmeth_efi_mgr      |   |-- efi_mgr
    bootmeth      3  [   ]   bootmeth_efi          |   |-- efi
    bootmeth      4  [   ]   bootmeth_cros         |   |-- cros
    bootmeth      5  [   ]   bootmeth_qfw          |   |-- qfw
    bootmeth      6  [   ]   bootmeth_pxe          |   |-- pxe
    bootmeth      7  [   ]   vbe_simple            |   `-- vbe_simple
    efi           0  [   ]   EFI block driver      |-- efi
    simple_bus    0  [   ]   cpu_bus               `-- cpus
    cpu           0  [   ]   cpu_qemu                  `-- cpu@0
    =>

This shows listing QEMU devices before and after scanning. Note that devices
which show '(none)' in the Driver column are not supported by U-Boot.

::

    => virtio list
    Name                  Type            Driver
    --------------------  --------------  ---------------
    virtio-pci.m#0         0: (unknown)   (none)
    virtio-pci.m#1         0: (unknown)   (none)
    virtio-pci.m#2         0: (unknown)   (none)
    virtio-pci.m#3         0: (unknown)   (none)
    virtio-pci.m#4         0: (unknown)   (none)
    virtio-pci.m#5         0: (unknown)   (none)
    virtio-pci.m#6         0: (unknown)   (none)
    virtio-pci.m#7         0: (unknown)   (none)
    virtio-pci.m#8         0: (unknown)   (none)
    virtio-pci.m#9         0: (unknown)   (none)
    virtio-pci.m#10        0: (unknown)   (none)
    => virtio scan
    => virtio list
    Name                  Type            Driver
    --------------------  --------------  ---------------
    virtio-pci.m#0         5: balloon     (none)
    virtio-pci.m#1         4: rng         (none)
    virtio-pci.m#2        12: input-host  (none)
    virtio-pci.m#3        12: input-host  (none)
    virtio-pci.m#4        13: vsock       (none)
    virtio-pci.m#5         3: serial      (none)
    virtio-pci.m#6         8: scsi        virtio-scsi#6
    virtio-pci.m#7         9: 9p          (none)
    virtio-pci.m#8        1a: fs          virtio-fs#8
    virtio-pci.m#9        10: gpu         (none)
    virtio-pci.m#10        1: net         virtio-net#a
    =>

This shows reading and displaying the partition table for device 0::

    => virtio part 0

    Partition Map for virtio device 0  --   Partition Type: EFI

    Part        Start LBA        End LBA                Name
            Attributes
            Type GUID
            Partition GUID
      1        0x00000800        0x00165fff        ""
            attrs:        0x0000000000000000
            type:        c12a7328-f81f-11d2-ba4b-00a0c93ec93b
                    (EFI System Partition)
            guid:        eccaac1c-b634-441b-a788-ab0f959111af
      2        0x00166000        0x01dff7ff        ""
            attrs:        0x0000000000000000
            type:        0fc63daf-8483-4772-8e79-3d69d8477de4
                    (linux)
            guid:        e71469a9-dba2-4f46-b959-75e732466bf8
    =>

This shows reading some data from device 1 (which is the README file)::

    => virtio dev 1

    Device 1: No filesystem (err -19)
    1af4 VirtIO Block Device
                Type: Hard Disk
                Capacity: 0.0 MB = 0.0 GB (171 x 512)
    ... is now current device
    => virtio read 1000 0 1

    virtio read: device 1 block # 0, count 1 ... 1 blocks read: OK
    => md 1000 20
    00001000: 50532023 4c2d5844 6e656369 492d6573  # SPDX-License-I
    00001010: 746e6564 65696669 47203a72 322d4c50  dentifier: GPL-2
    00001020: 0a2b302e 20230a23 20294328 79706f43  .0+.#.# (C) Copy
    00001030: 68676972 30322074 2d203030 31303220  right 2000 - 201
    00001040: 20230a33 666c6f57 676e6167 6e654420  3.# Wolfgang Den
    00001050: 44202c6b 20584e45 74666f53 65726177  k, DENX Software
    00001060: 676e4520 65656e69 676e6972 6477202c   Engineering, wd
    00001070: 6e656440 65642e78 530a0a2e 616d6d75  @denx.de...Summa
    =>

This shows modifying some data and writing it back. If you try this it will
modify the first character of the README file in your U-Boot tree::

    => virtio dev 1

    Device 1: No filesystem (err -19)
    1af4 VirtIO Block Device
                Type: Hard Disk
                Capacity: 0.0 MB = 0.0 GB (171 x 512)
    ... is now current device
    => mw.b 1000 61
    => md 1000 10
    00001000: 50532061 4c2d5844 6e656369 492d6573  a SPDX-License-I
    00001010: 746e6564 65696669 47203a72 322d4c50  dentifier: GPL-2
    00001020: 0a2b302e 20230a23 20294328 79706f43  .0+.#.# (C) Copy
    00001030: 68676972 30322074 2d203030 31303220  right 2000 - 201
    => virtio write 1000 0 1

    virtio write: device 1 block # 0, count 1 ... 1 blocks written: OK
    =>


Configuration
-------------

The `virtio` command is only available if `CONFIG_CMD_VIRTIO=y`.
