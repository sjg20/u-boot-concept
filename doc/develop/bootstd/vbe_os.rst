.. SPDX-License-Identifier: GPL-2.0+:

VBE OS Bootmeth
===============

VBE supports a separate FIT for devicetrees and the OS. This bootmeth detects
the `vbe-state` file on the media and uses this to determine the bootflow to
use: there may be two or three separate FITs, for the A, B and recovery flows.

In embedded systems it is common to package the devicetrees with the OS in
a FIT. Thus both can be updated together by writing a single file.

For distros which want to provide broad hardware support, it is sometimes
desirable to package the devicetrees separately from the OS, so they can have
their own OEM-controlled lifecycle. This allows the OS package to be somewhat
smalller than if it had all the devicetrees included.

In terms of lifecycle, the OEM may update the devicetree FIT as part of firmware
update, or a separate OS package.

There are challenges with this approach and it is best suited for fairly stable
platforms. Particularly in the early days of Linux supporting a particular
SoC, the devicetree/kernel interface may not be fully stable. It is often more
desirable to transition to using an OEM FIT once stability is achieved.

It is also possible to put small overlays in the OS FIT, e.g. to enable a
feature which was not working at launch. This results in smaller FITs tha
packaging the entire devicetree for each board.

The compatible string "vbe,abrec-os" is used for the driver. It is present
if `CONFIG_BOOTMETH_VBE_ABREC_OS` is enabled.

For more information on VBE, see :doc:`../vbe`.

Partition format
----------------

Boot files should be stored on a separate boot partition, formatted as ext4.

Each slot (A, B, recovery) has a subdirectory containing an
`extlinux/extlinux.conf` file. This provides information about the OS fit for
each OS version.

The VBE bootmeth requires a file called `vbe-state` to be present in the root
directory of the boot partition, to indicate which slot to boot.

The `vbe-state` file is a devicetree blob, with the following schema:

root node
    compatible: "vbe,abrec-state"

/os node:
    compatible: "vbe,os-state"

/os/next-boot:
    Indicates the slot that will be used on the next boot. It has a single
    property:

    slot (string)
        Indicates the slot that will be used on the next boot; valid values
        are "a", "b", and "recovery"
