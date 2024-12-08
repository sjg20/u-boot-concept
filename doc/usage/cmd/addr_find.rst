.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: addr_find (command)

addr_find command
=================

Synopsis
--------

::

    addr_find <interface> [<dev[:part]> [<filename>]]

Description
-----------

The addr_find command is used to find a consecutive region of memory
sufficiently large to hold a file, ensuring that the memory is not currently in
use for another file, etc.

If successful, 'loadaddr' is set to the located address.

The number of transferred bytes is saved in the environment variable filesize.
The load address is saved in the environment variable fileaddr.

interface
    interface for accessing the block device (mmc, sata, scsi, usb, ....)

dev
    device number

part
    partition number, defaults to 0 (whole device)

filename
    path to file, defaults to environment variable 'bootfile'

Example
-------

This shows obtaining an address suitable for a file on an mmc disk::

    => ls mmc 1
                extlinux/
     97135227   initramfs-5.3.7-301.fc31.armv7hl.img
                dtb-5.3.7-301.fc31.armv7hl/
     12531628   vmlinuz-5.3.7-301.fc31.armv7hl

    2 file(s), 2 dir(s)

    => addr_find mmc 1 vmlinuz-5.3.7-301.fc31.armv7hl
    => print loadaddr
    loadaddr=7c00000
    =>


Return value
------------

The return value $? is set to 0 (true) if the command succeeds. If no suitable
address could be found, the return value $? is set to 1 (false).
