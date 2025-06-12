# SPDX-License-Identifier: GPL-2.0+
#
"""Common script for build- scripts

"""

import configparser
import contextlib
import os
import shutil
import subprocess
import sys
import tempfile

OUR_PATH = os.path.dirname(os.path.realpath(__file__))
OUR1_PATH = os.path.dirname(OUR_PATH)

# Bring in the patman and test libraries (but don't override the first path in
# PYTHONPATH)
sys.path.insert(2, os.path.join(OUR1_PATH, 'tools'))
sys.path.insert(2, os.path.join(OUR1_PATH, 'test/py/tests'))

from u_boot_pylib import command
from u_boot_pylib import tools
import fs_helper


class Helper:
    def __init__(self):
        self.settings = None

    def read_settings(self):
        """Get settings from the settings file"""
        self.settings = configparser.ConfigParser()
        fname = f'{os.getenv("HOME")}/.u_boot_qemu'
        if not os.path.exists(fname):
            print(f'No config file found: {fname}\nCreating one...\n')
            tools.write_file(fname, '''# U-Boot QEMU-scripts config

[DEFAULT]
# Set ubdir to the build directory where you build U-Boot out-of-tree
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
''', binary=False)
        self.settings.read(fname)

    def get_setting(self, name, fallback=None):
        """Get a setting by name

        Args:
            name (str): Name of setting to retrieve
            fallback (str or None): Value to return if the setting is missing
        """
        raw = self.settings.get('DEFAULT', name, fallback=fallback)
        return os.path.expandvars(os.path.expanduser(raw))

    @contextlib.contextmanager
    def make_disk(self, fname, size_mb=20, fs_type='ext4', use_part=False):
        """Create a raw disk image with files on it

        Args:
            fname (str): Filename to write the images to
            fs_type (str): Filesystem type to create (ext4 or vfat)
            size_mb (int): Size in MiB
            use_part (bool): True to create a partition table, False to use a
                raw disk image

        Yields:
            str: Directory to write the files into
        """
        with tempfile.NamedTemporaryFile() as tmp:
            with tempfile.TemporaryDirectory(prefix='build_helper.') as dirname:
                try:
                    yield dirname
                    fs_helper.mk_fs(None, fs_type, size_mb << 20, None, dirname,
                                    fs_img=tmp.name, quiet=True)
                finally:
                    pass

            if use_part:
                with open(fname, 'wb') as img:
                    img.truncate(size_mb << 20)
                    img.seek(1 << 20, 0)
                    img.write(tools.read_file(tmp.name))
                subprocess.run(
                    ['sfdisk', fname], text=True, check=True,
                    capture_output=True,
                    input=f'type=c, size={size_mb-1}M, start=1M,bootable')
            else:
                shutil.copy2(tmp.name, fname)


def add_common_args(parser):
    """Add some arguments which are common to build-efi/qemu scripts

    Args:
        parser (argparse.ArgumentParser): Parser to modify
    """
    parser.add_argument('-a', '--arch', default='arm', choices=['arm', 'x86'],
                        help='Select architecture (arm, x86) Default: arm')
    parser.add_argument('-B', '--no-build', action='store_true',
                        help="Don't build; assume a build exists")
    parser.add_argument('-d', '--disk',
                        help='Root disk image file to use with QEMU')
    parser.add_argument(
        '-k', '--kvm', action='store_true',
        help='Use KVM (Kernel-based Virtual Machine) for acceleration')
    parser.add_argument('-o', '--os', metavar='NAME', choices=['ubuntu'],
                        help='Run a specified Operating System')
    parser.add_argument('-r', '--run', action='store_true',
                        help='Run QEMU with the image')
    parser.add_argument(
        '-R', '--release', default='24.04.1',
        help='Select OS release version (e.g, 24.04) Default: 24.04.1')
    parser.add_argument('-s', '--serial-only', action='store_true',
                        help='Run QEMU with serial only (no display)')
    parser.add_argument('-w', '--word-32bit', action='store_true',
                        help='Use 32-bit version for the build/architecture')
