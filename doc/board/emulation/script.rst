.. SPDX-License-Identifier: GPL-2.0+

Script for building and running
===============================

You may find the script `scripts/build-qemu` helpful for building and testing
U-Boot on QEMU.

If uses a settings file `~/.u_boot_qemu` to control how it works:

build_dir
    base directory for building U-Boot, with each board being in its own
    subdirectory

image_dir
    directory containing OS images, containing a subdirectory for each distro
    type (e.g. `ubuntu/`)

bzimage
    path to a bzImage file to supply to boot x86 Linux

efi_image_file
    output filename for the disk image containing an EFI app / payload

efi_dir
    directory when pre-built UEFI images are kept, e.g. OVMF-pure-efi.i386.fd

sct_dir
    directory when the UEFI Self-Certification Test (SCT) is kept

sct_mnt
    temporary mount point for building SCT: note this requires sudo

A sample file is written if you don't have one, e.g.::

    # U-Boot QEMU-scripts config

    [DEFAULT]
    # Set to the build directory where you build U-Boot out-of-tree
    # We avoid in-tree build because it gets confusing trying different builds
    # Each board gets a build in a separate subdir
    build_dir = /tmp/b

    # Image directory (for OS images)
    image_dir = ~/dev/os

    # Build the kernel with: make O=/tmp/kernel
    bzimage = /tmp/kernel/arch/x86/boot/bzImage

    # EFI image-output filename
    efi_image_file = try.img

    # Directory where OVMF-pure-efi.i386.fd etc. are kept
    efi_dir = ~/dev/efi

    # Directory where SCT image (sct.img) is kept
    sct_dir = ~/dev/efi/sct

    # Directory where the SCT image is temporarily mounted for modification
    sct_mnt = /mnt/sct


Once configured, you can build and run QEMU for arm64 like this::

    scripts/build-qemu -rsw

To pass a custom boot command to U-Boot via fw_cfg, use the `--bootcmd`
option::

    scripts/build-qemu -rsw --bootcmd "echo Hello from QEMU; bootflow scan -lb"

This will cause U-Boot to execute the specified command instead of the default
autoboot behavior.

Options
~~~~~~~

Options are available to control the script:

-a <arch>
    Select architecture (default arm, x86)

-B
    Don't build; assume a build exists

-d/--disk DISK
    Root disk image file to use with QEMU

-e/--sct-run
    Package an run UEFI Self-Certification Test (SCT)

-E/--use-tianocore
    Run Tianocore (OVMF) instead of U-Boot

-k
    Use kvm - kernel-based Virtual Machine. By default QEMU uses its own
    emulator

-o <os>
    Run an Operating System. For now this only supports 'ubuntu'. The name of
    the OS file must remain unchanged from its standard name on the Ubuntu
    website.

-r
    Run QEMU with the image (by default this is not done)

-R
    Select OS release (e.g. 24.04).

-s
    Use serial only (no display)

-S/--sct-seq SCT_SEQ
    SCT sequence-file to be written into the SCT image if -e

-w
    Use word version (32-bit). By default, 64-bit is used

--bootcmd BOOTCMD
    U-Boot bootcmd to pass via fw_cfg. This allows passing a custom boot
    command to U-Boot at runtime through QEMU's firmware configuration
    interface. The bootcmd is written to the 'opt/u-boot/bootcmd' fw_cfg
    entry and is read by U-Boot's EVT_BOOTCMD handler before the default
    autoboot process runs.
