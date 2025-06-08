.. SPDX-License-Identifier: GPL-2.0+:

QFW Bootmeth
============

`QEMU <hhttps://www.qemu.org/>`_ is a system emulator which is able to boot
Operating Systems. QEMU provides specific support for booting an OS image
provided on the QEMU command line.

When invoked on a bootdev for UCLASS_QFW, this bootmeth reads the kernel
provided by the QEMU `-kernel` argument, the initial ramdisk provided by
`-initrd` and the boot arguments (command line) provided by `-append` into
memory ready for booting.

When the bootflow is booted, the bootmeth tries the `booti` command first, then
falls back to the `bootz` command, then `zboot`. U-Boot's 'control' devicetree
is passed through to the kernel on non-x86 devices.

The `bootflow read` command is supported, so it is possible to read the files
and then check the kernel command-line before using `bootflow boot` to boot.

The compatible string "u-boot,qfw-bootmeth" is used for the driver. It is
present if `CONFIG_QFW` is enabled.
