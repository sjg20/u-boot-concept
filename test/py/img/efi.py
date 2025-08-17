# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.

"""Create EFI test disk images"""

import os

from fs_helper import DiskHelper, FsHelper
from img.common import mkdir_cond


def setup_efi_image(config):
    """Create a 20MB disk image with an EFI app on it

    Args:
        config (ArbitraryAttributeContainer): Configuration
    """
    devnum = 1
    fsh = FsHelper(config, 'vfat', 18, 'flash')
    fsh.setup()
    efi_dir = os.path.join(fsh.srcdir, 'EFI')
    mkdir_cond(efi_dir)
    bootdir = os.path.join(efi_dir, 'BOOT')
    mkdir_cond(bootdir)
    efi_src = os.path.join(config.build_dir,
                        'lib/efi_loader/testapp.efi')
    efi_dst = os.path.join(bootdir, 'BOOTSBOX.EFI')
    with open(efi_src, 'rb') as inf:
        with open(efi_dst, 'wb') as outf:
            outf.write(inf.read())

    fsh.mk_fs()

    img = DiskHelper(config, devnum, 'flash', True)
    img.add_fs(fsh, DiskHelper.VFAT)
    img.create()
    fsh.cleanup()