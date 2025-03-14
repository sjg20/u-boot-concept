bootctl - Boot Schema
=====================

Introduction
------------

This is a very basic prototype which aims to show some of the concepts behind
the 'boot schema' idea and how they can be implemented in practice.

Please see the FO215 document for details on the schema.

Features
--------

Very, very few features are supported:
- basic menu
- finding OSes for the menu (only extlinux.conf and EFI are supported)
- measurement of images using a TPM

Running on QEMU
---------------

To run this, first create an image with Ubuntu 2024.04. This script can help
but you will need to edit some variables at the top (imagedir and mnt) or pass
these vars into the script.

This runs the installer:

   ./scripts/build-qemu.sh -a x86 -r -k -d root.img -R 24.04

Go through the full install and then reboot.

Then run without the OS image:

   ./scripts/build-qemu.sh -a x86 -r -k -d root.img

Now you can install the u-boot-tools package so that an extlinux.conf file is
created on the disk.

When you reboot you should see a bootmenu with a few options.

If you know U-Boot well you can probably run on sandbox


Where is the schema?
--------------------

For now the schema is in `include/bootctl.dtsi` and is in devicetree format.
We will likely use YAML for this, although it may be useful to then compile the
YAML into devicetree in some cases.


Where is the boot logic?
------------------------

See `boot/bootctl/bootctl.c` for the top-level program. It really just gets the
logic driver and polls it until it either boots or gives up.

The real logic is in `boot/bootctl/logic.c`. The `logic_start()` function sets
things up, then `logic_poll()` actually manages finding things to boot and
sending them to the UI.

The data between `logic.c` and `ui.c` is a bit messy and can likely be tidied
up.

Source map
----------

boot/bootctl
    Directory containing the source for use with U-Boot

cmd/
    Provides a very simple 'bootctl' command to start things up

include/bootctl
    Include files for bootctl

test/boot/bootctl
    A few very simple tests to give a flavour of how tests might work

What about all the other patches related to expo? Please just ignore these. My
original prototype was terribly ugly so I spent some time trying to clean it
up.

Comments
--------

Please send any and all comments and suggestions to me.


--
Simon Glass
26-Mar-25
