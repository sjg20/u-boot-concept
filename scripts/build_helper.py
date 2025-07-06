# SPDX-License-Identifier: GPL-2.0+
#
"""Common script for build- scripts

"""

import configparser
import contextlib
import os
from pathlib import Path
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
from u_boot_pylib import tout
import fs_helper

MODERN_PCI = 'disable-legacy=on,disable-modern=off'


class Helper:
    def __init__(self, args):
        self.settings = None
        self.imagedir = None
        self.args = args
        self.bitness = 32 if args.word_32bit else 64
        if self.args.arch == 'arm':
            if self.bitness == 64:
                self.os_arch = 'arm64'
            else:
                self.os_arch = 'arm'
        else:  # x86
            if self.bitness == 64:
                self.os_arch = 'amd64'
            else:
                self.os_arch = 'i386'

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
        self.imagedir = Path(self.get_setting('image_dir', '~/dev'))

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

    def add_qemu_args(self, args, cmd):
        """Add QEMU arguments according to the selected options

        This helps in creating the command-line used to run QEMU.

        Args:
            args (list of str): Existing arguments to add to
            cmd (argparse.Namespace): Program arguments
        """
        cmdline = []
        if args.kernel:
            cmd.extend(['-kernel', args.kernel])
        if args.initrd:
            cmd.extend(['-initrd', args.initrd])

        if args.enable_console:
            cmdline.append('console=ttyS0,115200,8n1')
        if args.root:
            cmdline.append(f'root={args.root}')
        if args.uuid:
            cmdline.append(f'root=/dev/disk/by-uuid/{args.uuid}')

        if cmdline:
            cmd.extend(['-append'] + [' '.join(cmdline)])

        os_path = None
        if args.os == 'ubuntu':
            img_name = f'{args.os}-{args.release}-desktop-{self.os_arch}.iso'
            os_path = self.imagedir / args.os / img_name
            if not os_path.exists():
                tout.error(f'OS image {os_path} specified but not found')
            else:
                cmd.extend([
                    '-drive',
                    f'if=virtio,file={os_path},format=raw,id=hd0,readonly=on'])

        for i, d in enumerate(args.disk):
            disk = Path(d)
            if disk.exists():
                iface = 'none' if args.scsi else 'virtio'
                if args.scsi:
                    cmd.extend([
                        '-device',
                        f'virtio-scsi-pci,id=scsi0,{MODERN_PCI}',
                        '-device', f'scsi-hd,bus=scsi0.0,drive=hd{i}'])
                cmd.extend([
                    '-drive',
                    f'if={iface},file={disk},format=raw,id=hd{i}'])
            else:
                tout.warning(f"Disk image '{disk}' not found")

def add_common_args(parser):
    """Add some arguments which are common to build-efi/qemu scripts

    Args:
        parser (argparse.ArgumentParser): Parser to modify
    """
    parser.add_argument('-a', '--arch', default='arm', choices=['arm', 'x86'],
                        help='Select architecture (arm, x86) Default: arm')
    parser.add_argument('-B', '--no-build', action='store_true',
                        help="Don't build; assume a build exists")
    parser.add_argument('-C', '--enable-console', action='store_true',
                        help="Enable linux console (x86 only)")
    parser.add_argument('-d', '--disk', nargs='*',
                        help='Root disk image file to use with QEMU')
    parser.add_argument('-I', '--initrd',
                        help='Initial ramdisk to run using -initrd')
    parser.add_argument(
        '-k', '--kvm', action='store_true',
        help='Use KVM (Kernel-based Virtual Machine) for acceleration')
    parser.add_argument('-K', '--kernel',
                        help='Kernel to run using -kernel')
    parser.add_argument('-o', '--os', metavar='NAME', choices=['ubuntu'],
                        help='Run a specified Operating System')
    parser.add_argument('-r', '--run', action='store_true',
                        help='Run QEMU with the image')
    parser.add_argument(
        '-R', '--release', default='24.04.1',
        help='Select OS release version (e.g, 24.04) Default: 24.04.1')
    parser.add_argument('-s', '--serial-only', action='store_true',
                        help='Run QEMU with serial only (no display)')
    parser.add_argument(
        '-S', '--scsi', action='store_true',
        help='Attach root disk using virtio-sci instead of virtio-blk')
    parser.add_argument(
        '-t', '--root',
        help='Pass the given root device to linux via root=xxx')
    parser.add_argument(
        '-U', '--uuid',
        help='Pass the given root device to linux via root=/dev/disk/by-uuid/')
    parser.add_argument('-w', '--word-32bit', action='store_true',
                        help='Use 32-bit version for the build/architecture')
