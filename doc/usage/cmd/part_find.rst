.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: part_find (command)

part_find command
=================

Synopsis
--------

::

    part_find <uuid>
    part_find self

Description
-----------

The `part_find` command is used to find a partition with a given type GUID. When
it finds one, it sets the target_part environment variable to the corresponding
``interface dev:part`` string.

uuid
    Universally Unique Identifier (UUID) to search, expressed as a string

self
    This is only permitted in the EFI app. It indicates that the required
    partition is the one from which the app was started.

Example
-------

This shows searching for an EFI system partition and looking at the files on
that partition::

    => host bind 1 mmc5.img
    => part list host 0

    Partition Map for host device 0  --   Partition Type: EFI

    Part    Start LBA       End LBA         Name
            Attributes
            Type GUID
            Partition GUID
    1     0x0000202f      0x0000282e      ""
            attrs:  0x0000000000000000
            type:   ebd0a0a2-b9e5-4433-87c0-68b6b72699c7
                    (data)
            guid:   6b1e51e3-427c-9f45-a947-e467b7216356
    2     0x0000002d      0x0000082c      ""
            attrs:  0x0000000000000000
            type:   fe3a2a5d-4f32-41a7-b725-accc3285a309
                    (cros-kern)
            guid:   dece619f-4876-e140-a6c9-8c208a0c9099
    3     0x0000202e      0x0000202e      ""
            attrs:  0x0000000000000000
            type:   3cb8e202-3b7e-47dd-8a3c-7ff2a13cfcec
                    (cros-root)
            guid:   078cee87-a195-ae4c-a974-8ba3a3d783b3
    4     0x0000082d      0x0000102c      ""
            attrs:  0x0000000000000000
            type:   fe3a2a5d-4f32-41a7-b725-accc3285a309
                    (cros-kern)
            guid:   08d2f20f-d941-fc43-96f6-948931289d71
    5     0x0000202d      0x0000202d      ""
            attrs:  0x0000000000000000
            type:   3cb8e202-3b7e-47dd-8a3c-7ff2a13cfcec
                    (cros-root)
            guid:   0b23ba00-a11c-ed4e-8b49-5e8738899569
    6     0x00000029      0x00000029      ""
            attrs:  0x0000000000000000
            type:   fe3a2a5d-4f32-41a7-b725-accc3285a309
                    (cros-kern)
            guid:   6d8158a8-f82d-0d4d-8983-a3ada4eb9b73
    7     0x0000002a      0x0000002a      ""
            attrs:  0x0000000000000000
            type:   3cb8e202-3b7e-47dd-8a3c-7ff2a13cfcec
                    (cros-root)
            guid:   76e8f9b0-7db7-3844-8f18-21de93485211
    8     0x0000102d      0x0000182c      ""
            attrs:  0x0000000000000000
            type:   ebd0a0a2-b9e5-4433-87c0-68b6b72699c7
                    (data)
            guid:   071dfd2d-173c-f64b-9474-3318665e1d24
    9     0x0000002b      0x0000002b      ""
            attrs:  0x0000000000000000
            type:   2e0a753d-9e48-43b0-8337-b15192cb1b5e
                    (cros-rsrv)
            guid:   b9d078c3-bafa-cd48-b771-a0aaa18d5008
    10     0x0000002c      0x0000002c      ""
            attrs:  0x0000000000000000
            type:   2e0a753d-9e48-43b0-8337-b15192cb1b5e
                    (cros-rsrv)
            guid:   7b0c0234-1a29-0c4f-bceb-40fae8f7b27c
    11     0x00000028      0x00000028      ""
            attrs:  0x0000000000000000
            type:   cab6e88e-abf3-4102-a07a-d4bb9be3c1d3
                    (cros-fw)
            guid:   aced715d-cd1f-394a-9e3e-24b54a7b1472
    12     0x0000182d      0x0000202c      ""
            attrs:  0x0000000000000000
            type:   c12a7328-f81f-11d2-ba4b-00a0c93ec93b
                    (system)
            guid:   e1672afd-75ee-d74e-be95-8726b12b5e74
    => part_find c12a7328-f81f-11d2-ba4b-00a0c93ec93b
    => print target_part
    target_part=host 0:c
    => ls $target_part
                EFI/

    0 file(s), 1 dir(s)


Return value
------------

The return value $? is set to 0 (true) if the command succeeds. If no partition
could be found, the return value $? is set to 1 (false).
