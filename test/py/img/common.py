# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.

"""Common utilities for image creation"""

import os

import utils
from fs_helper import DiskHelper, FsHelper


def mkdir_cond(dirname):
    """Create a directory if it doesn't already exist

    Args:
        dirname (str): Name of directory to create
    """
    if not os.path.exists(dirname):
        os.mkdir(dirname)


def copy_partition(ubman, fsfile, outname):
    """Copy a partition into a disk image

    Args:
        ubman (ConsoleBase): U-Boot fixture
        fsfile (str): Name of partition file
        outname (str): Name of full-disk file to update
    """
    utils.run_and_log(ubman,
                      f'dd if={fsfile} of={outname} bs=1M seek=1 conv=notrunc')


def setup_extlinux_image(config, log, devnum, basename, vmlinux, initrd, dtbdir,
                         script):
    """Create a 20MB disk image with a single FAT partition

    Args:
        config (ArbitraryAttributeContainer): Configuration
        log (multiplexed_log.Logfile): Log to write to
        devnum (int): Device number to use, e.g. 1
        basename (str): Base name to use in the filename, e.g. 'mmc'
        vmlinux (str): Kernel filename
        initrd (str): Ramdisk filename
        dtbdir (str or None): Devicetree filename
        script (str): Script to place in the extlinux.conf file
    """
    import gzip
    
    fsh = FsHelper(config, 'vfat', 18, prefix=basename)
    fsh.setup()

    ext = os.path.join(fsh.srcdir, 'extlinux')
    mkdir_cond(ext)

    conf = os.path.join(ext, 'extlinux.conf')
    with open(conf, 'w', encoding='ascii') as fd:
        print(script, file=fd)

    inf = os.path.join(config.persistent_data_dir, 'inf')
    with open(inf, 'wb') as fd:
        fd.write(gzip.compress(b'vmlinux'))
    mkimage = config.build_dir + '/tools/mkimage'
    utils.run_and_log_no_ubman(
        log, f'{mkimage} -f auto -d {inf} {os.path.join(fsh.srcdir, vmlinux)}')

    with open(os.path.join(fsh.srcdir, initrd), 'w', encoding='ascii') as fd:
        print('initrd', file=fd)

    if dtbdir:
        mkdir_cond(os.path.join(fsh.srcdir, dtbdir))

        dtb_file = os.path.join(fsh.srcdir, f'{dtbdir}/sandbox.dtb')
        utils.run_and_log_no_ubman(
            log, f'dtc -o {dtb_file}', stdin=b'/dts-v1/; / {};')

    fsh.mk_fs()

    img = DiskHelper(config, devnum, basename, True)
    img.add_fs(fsh, DiskHelper.VFAT, bootable=True)

    ext4 = FsHelper(config, 'ext4', 1, prefix=basename)
    ext4.setup()
    ext4.mk_fs()

    img.add_fs(ext4, DiskHelper.EXT4)
    img.create()
    fsh.cleanup()