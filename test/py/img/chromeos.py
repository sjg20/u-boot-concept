# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.

"""Create ChromeOS test disk images"""

import collections
import os

import utils


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