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


def setup_cros_image(config, log):
    """Create a 20MB disk image with ChromiumOS partitions

    Args:
        config (ArbitraryAttributeContainer): Configuration
        log (multiplexed_log.Logfile): Log to write to
    """
    Partition = collections.namedtuple('part', 'start,size,name')
    parts = {}
    disk_data = None

    def pack_kernel(config, log, arch, kern, dummy):
        """Pack a kernel containing some fake data

        Args:
            config (ArbitraryAttributeContainer): Configuration
            log (multiplexed_log.Logfile): Log to write to
            arch (str): Architecture to use ('x86' or 'arm')
            kern (str): Filename containing kernel
            dummy (str): Dummy filename to use for config and bootloader

        Return:
            bytes: Packed-kernel data
        """
        kern_part = os.path.join(config.result_dir, f'kern-part-{arch}.bin')
        utils.run_and_log_no_ubman(
            log,
            f'futility vbutil_kernel --pack {kern_part} '
            '--keyblock doc/chromium/files/devkeys/kernel.keyblock '
            '--signprivate doc/chromium/files/devkeys/kernel_data_key.vbprivk '
            f'--version 1  --config {dummy} --bootloader {dummy} '
            f'--vmlinuz {kern}')

        with open(kern_part, 'rb') as inf:
            kern_part_data = inf.read()
        return kern_part_data

    def set_part_data(partnum, data):
        """Set the contents of a disk partition

        This updates disk_data by putting data in the right place

        Args:
            partnum (int): Partition number to set
            data (bytes): Data for that partition
        """
        nonlocal disk_data

        start = parts[partnum].start * sect_size
        disk_data = disk_data[:start] + data + disk_data[start + len(data):]

    mmc_dev = 5
    fname = os.path.join(config.source_dir, f'mmc{mmc_dev}.img')
    utils.run_and_log_no_ubman(log, f'qemu-img create {fname} 20M')
    utils.run_and_log_no_ubman(log, f'cgpt create {fname}')

    uuid_state = 'ebd0a0a2-b9e5-4433-87c0-68b6b72699c7'
    uuid_kern = 'fe3a2a5d-4f32-41a7-b725-accc3285a309'
    uuid_root = '3cb8e202-3b7e-47dd-8a3c-7ff2a13cfcec'
    uuid_rwfw = 'cab6e88e-abf3-4102-a07a-d4bb9be3c1d3'
    uuid_reserved = '2e0a753d-9e48-43b0-8337-b15192cb1b5e'
    uuid_efi = 'c12a7328-f81f-11d2-ba4b-00a0c93ec93b'

    ptr = 40

    # Number of sectors in 1MB
    sect_size = 512
    sect_1mb = (1 << 20) // sect_size

    required_parts = [
        {'num': 0xb, 'label':'RWFW', 'type': uuid_rwfw, 'size': '1'},
        {'num': 6, 'label':'KERN_C', 'type': uuid_kern, 'size': '1'},
        {'num': 7, 'label':'ROOT_C', 'type': uuid_root, 'size': '1'},
        {'num': 9, 'label':'reserved', 'type': uuid_reserved, 'size': '1'},
        {'num': 0xa, 'label':'reserved', 'type': uuid_reserved, 'size': '1'},

        {'num': 2, 'label':'KERN_A', 'type': uuid_kern, 'size': '1M'},
        {'num': 4, 'label':'KERN_B', 'type': uuid_kern, 'size': '1M'},

        {'num': 8, 'label':'OEM', 'type': uuid_state, 'size': '1M'},
        {'num': 0xc, 'label':'EFI-SYSTEM', 'type': uuid_efi, 'size': '1M'},

        {'num': 5, 'label':'ROOT_B', 'type': uuid_root, 'size': '1'},
        {'num': 3, 'label':'ROOT_A', 'type': uuid_root, 'size': '1'},
        {'num': 1, 'label':'STATE', 'type': uuid_state, 'size': '1M'},
        ]

    for part in required_parts:
        size_str = part['size']
        if 'M' in size_str:
            size = int(size_str[:-1]) * sect_1mb
        else:
            size = int(size_str)
        utils.run_and_log_no_ubman(
            log,
            f"cgpt add -i {part['num']} -b {ptr} -s {size} -t {part['type']} {fname}")
        ptr += size

    utils.run_and_log_no_ubman(log, f'cgpt boot -p {fname}')
    out = utils.run_and_log_no_ubman(log, f'cgpt show -q {fname}')

    # We expect something like this:
    #   8239        2048       1  Basic data
    #     45        2048       2  ChromeOS kernel
    #   8238           1       3  ChromeOS rootfs
    #   2093        2048       4  ChromeOS kernel
    #   8237           1       5  ChromeOS rootfs
    #     41           1       6  ChromeOS kernel
    #     42           1       7  ChromeOS rootfs
    #   4141        2048       8  Basic data
    #     43           1       9  ChromeOS reserved
    #     44           1      10  ChromeOS reserved
    #     40           1      11  ChromeOS firmware
    #   6189        2048      12  EFI System Partition

    # Create a dict (indexed by partition number) containing the above info
    for line in out.splitlines():
        start, size, num, name = line.split(maxsplit=3)
        parts[int(num)] = Partition(int(start), int(size), name)

    # Set up the kernel command-line
    dummy = os.path.join(config.result_dir, 'dummy.txt')
    with open(dummy, 'wb') as outf:
        outf.write(b'BOOT_IMAGE=/vmlinuz-5.15.0-121-generic root=/dev/nvme0n1p1 ro quiet splash vt.handoff=7')

    # For now we just use dummy kernels. This limits testing to just detecting
    # a signed kernel. We could add support for the x86 data structures so that
    # testing could cover getting the cmdline, setup.bin and other pieces.
    kern = os.path.join(config.result_dir, 'kern.bin')
    with open(kern, 'wb') as outf:
        outf.write(b'kernel\n')

    with open(fname, 'rb') as inf:
        disk_data = inf.read()

    # put x86 kernel in partition 2 and arm one in partition 4
    set_part_data(2, pack_kernel(config, log, 'x86', kern, dummy))
    set_part_data(4, pack_kernel(config, log, 'arm', kern, dummy))

    with open(fname, 'wb') as outf:
        outf.write(disk_data)

    return fname

def setup_android_image(config, log):
    """Create a 20MB disk image with Android partitions

    Args:
        config (ArbitraryAttributeContainer): Configuration
        log (multiplexed_log.Logfile): Log to write to
    """
    Partition = collections.namedtuple('part', 'start,size,name')
    parts = {}
    disk_data = None

    def set_part_data(partnum, data):
        """Set the contents of a disk partition

        This updates disk_data by putting data in the right place

        Args:
            partnum (int): Partition number to set
            data (bytes): Data for that partition
        """
        nonlocal disk_data

        start = parts[partnum].start * sect_size
        disk_data = disk_data[:start] + data + disk_data[start + len(data):]

    mmc_dev = 7
    fname = os.path.join(config.source_dir, f'mmc{mmc_dev}.img')
    utils.run_and_log_no_ubman(log, f'qemu-img create {fname} 20M')
    utils.run_and_log_no_ubman(log, f'cgpt create {fname}')

    ptr = 40

    # Number of sectors in 1MB
    sect_size = 512
    sect_1mb = (1 << 20) // sect_size

    required_parts = [
        {'num': 1, 'label':'misc', 'size': '1M'},
        {'num': 2, 'label':'boot_a', 'size': '4M'},
        {'num': 3, 'label':'boot_b', 'size': '4M'},
        {'num': 4, 'label':'vendor_boot_a', 'size': '4M'},
        {'num': 5, 'label':'vendor_boot_b', 'size': '4M'},
    ]

    for part in required_parts:
        size_str = part['size']
        if 'M' in size_str:
            size = int(size_str[:-1]) * sect_1mb
        else:
            size = int(size_str)
        utils.run_and_log_no_ubman(
            log,
            f"cgpt add -i {part['num']} -b {ptr} -s {size} -l {part['label']} -t basicdata {fname}")
        ptr += size

    utils.run_and_log_no_ubman(log, f'cgpt boot -p {fname}')
    out = utils.run_and_log_no_ubman(log, f'cgpt show -q {fname}')

    # Create a dict (indexed by partition number) containing the above info
    for line in out.splitlines():
        start, size, num, name = line.split(maxsplit=3)
        parts[int(num)] = Partition(int(start), int(size), name)

    with open(fname, 'rb') as inf:
        disk_data = inf.read()

    test_abootimg.AbootimgTestDiskImage(config, log, 'bootv4.img',
                                        test_abootimg.boot_img_hex)
    boot_img = os.path.join(config.result_dir, 'bootv4.img')
    with open(boot_img, 'rb') as inf:
        set_part_data(2, inf.read())

    test_abootimg.AbootimgTestDiskImage(config, log, 'vendor_boot.img',
                                        test_abootimg.vboot_img_hex)
    vendor_boot_img = os.path.join(config.result_dir, 'vendor_boot.img')
    with open(vendor_boot_img, 'rb') as inf:
        set_part_data(4, inf.read())

    with open(fname, 'wb') as outf:
        outf.write(disk_data)

    print(f'wrote to {fname}')

    mmc_dev = 8
    fname = os.path.join(config.source_dir, f'mmc{mmc_dev}.img')
    utils.run_and_log_no_ubman(log, f'qemu-img create {fname} 20M')
    utils.run_and_log_no_ubman(log, f'cgpt create {fname}')

    ptr = 40

    # Number of sectors in 1MB
    sect_size = 512
    sect_1mb = (1 << 20) // sect_size

    required_parts = [
        {'num': 1, 'label':'misc', 'size': '1M'},
        {'num': 2, 'label':'boot_a', 'size': '4M'},
        {'num': 3, 'label':'boot_b', 'size': '4M'},
    ]

    for part in required_parts:
        size_str = part['size']
        if 'M' in size_str:
            size = int(size_str[:-1]) * sect_1mb
        else:
            size = int(size_str)
        utils.run_and_log_no_ubman(
            log,
            f"cgpt add -i {part['num']} -b {ptr} -s {size} -l {part['label']} -t basicdata {fname}")
        ptr += size

    utils.run_and_log_no_ubman(log, f'cgpt boot -p {fname}')
    out = utils.run_and_log_no_ubman(log, f'cgpt show -q {fname}')

    # Create a dict (indexed by partition number) containing the above info
    for line in out.splitlines():
        start, size, num, name = line.split(maxsplit=3)
        parts[int(num)] = Partition(int(start), int(size), name)

    with open(fname, 'rb') as inf:
        disk_data = inf.read()

    test_abootimg.AbootimgTestDiskImage(config, log, 'boot.img',
                                        test_abootimg.img_hex)
    boot_img = os.path.join(config.result_dir, 'boot.img')
    with open(boot_img, 'rb') as inf:
        set_part_data(2, inf.read())

    with open(fname, 'wb') as outf:
        outf.write(disk_data)

    print(f'wrote to {fname}')

    return fname

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
