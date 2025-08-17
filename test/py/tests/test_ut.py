# SPDX-License-Identifier: GPL-2.0
"""
Unit-test runner

Provides a test_ut() function which is used by conftest.py to run each unit
test one at a time, as well setting up some files needed by the tests.

# Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
"""
import collections
import gzip
import os
import os.path
import pytest

import utils
# pylint: disable=E0611
from fs_helper import DiskHelper, FsHelper
from test_android import test_abootimg
from img.vbe import setup_vbe_image
from img.common import mkdir_cond, copy_partition, setup_extlinux_image
from img.fedora import setup_fedora_image
from img.ubuntu import setup_ubuntu_image
from img.armbian import setup_bootmenu_image
from img.chromeos import setup_cros_image
from img.android import setup_android_image


def setup_cedit_file(config, log):
    """Set up a .dtb file for use with testing expo and configuration editor

    Args:
        config (ArbitraryAttributeContainer): Configuration
        log (multiplexed_log.Logfile): Log to write to
    """
    infname = os.path.join(config.source_dir,
                           'test/boot/files/expo_layout.dts')
    inhname = os.path.join(config.source_dir,
                           'test/boot/files/expo_ids.h')
    expo_tool = os.path.join(config.source_dir, 'tools/expo.py')
    outfname = 'cedit.dtb'
    utils.run_and_log_no_ubman(
        log, f'{expo_tool} -e {inhname} -l {infname} -o {outfname}')

@pytest.mark.buildconfigspec('ut_dm')
def test_ut_dm_init(ubman):
    """Initialize data for ut dm tests."""

    fn = ubman.config.source_dir + '/testflash.bin'
    if not os.path.exists(fn):
        data = b'this is a test'
        data += b'\x00' * ((4 * 1024 * 1024) - len(data))
        with open(fn, 'wb') as fh:
            fh.write(data)

    fn = ubman.config.source_dir + '/spi.bin'
    if not os.path.exists(fn):
        data = b'\x00' * (2 * 1024 * 1024)
        with open(fn, 'wb') as fh:
            fh.write(data)

    # Create a file with a single partition
    fn = ubman.config.source_dir + '/scsi.img'
    if not os.path.exists(fn):
        data = b'\x00' * (2 * 1024 * 1024)
        with open(fn, 'wb') as fh:
            fh.write(data)
        utils.run_and_log(
            ubman, f'sfdisk {fn}', stdin=b'type=83')

    FsHelper(ubman.config, 'ext2', 2, '2MB').mk_fs()
    FsHelper(ubman.config, 'fat32', 1, '1MB').mk_fs()

    mmc_dev = 6
    fn = os.path.join(ubman.config.source_dir, f'mmc{mmc_dev}.img')
    data = b'\x00' * (12 * 1024 * 1024)
    with open(fn, 'wb') as fh:
        fh.write(data)


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


def setup_localboot_image(config, log):
    """Create a 20MB disk image with a single FAT partition

    Args:
        config (ArbitraryAttributeContainer): Configuration
        log (multiplexed_log.Logfile): Log to write to
    """
    mmc_dev = 9

    script = '''DEFAULT local

LABEL local
  SAY Doing local boot...
  LOCALBOOT 0
'''
    vmlinux = 'vmlinuz'
    initrd = 'initrd.img'
    setup_extlinux_image(config, log, mmc_dev, 'mmc', vmlinux, initrd, None,
                         script)


@pytest.mark.buildconfigspec('cmd_bootflow')
@pytest.mark.buildconfigspec('sandbox')
def test_ut_dm_init_bootstd(u_boot_config, u_boot_log):
    """Initialise data for bootflow tests

    Args:
        u_boot_config (ArbitraryAttributeContainer): Configuration
        u_boot_log (multiplexed_log.Logfile): Log to write to
    """
    setup_fedora_image(u_boot_config, u_boot_log, 1, 'mmc')
    setup_bootmenu_image(u_boot_config, u_boot_log)
    setup_cedit_file(u_boot_config, u_boot_log)
    setup_cros_image(u_boot_config, u_boot_log)
    setup_android_image(u_boot_config, u_boot_log)
    setup_efi_image(u_boot_config)
    setup_ubuntu_image(u_boot_config, u_boot_log, 3, 'flash')
    setup_localboot_image(u_boot_config, u_boot_log)
    setup_vbe_image(u_boot_config, u_boot_log)

def test_ut(ubman, ut_subtest):
    """Execute a "ut" subtest.

    The subtests are collected in function generate_ut_subtest() from linker
    generated lists by applying a regular expression to the lines of file
    u-boot.sym. The list entries are created using the C macro UNIT_TEST().

    Strict naming conventions have to be followed to match the regular
    expression. Use UNIT_TEST(foo_test_bar, _flags, foo_test) for a test bar in
    test suite foo that can be executed via command 'ut foo bar' and is
    implemented in C function foo_test_bar().

    Args:
        ubman (ConsoleBase): U-Boot console
        ut_subtest (str): test to be executed via command ut, e.g 'foo bar' to
            execute command 'ut foo bar'
    """

    if ut_subtest == 'hush hush_test_simple_dollar':
        # ut hush hush_test_simple_dollar prints "Unknown command" on purpose.
        with ubman.disable_check('unknown_command'):
            output = ubman.run_command('ut ' + ut_subtest)
        assert 'Unknown command \'quux\' - try \'help\'' in output
    else:
        output = ubman.run_command('ut ' + ut_subtest)
    assert output.endswith('failures: 0')
