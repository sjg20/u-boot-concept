#!/usr/bin/env python
# SPDX-License-Identifier: GPL-2.0+
"""
Script to build an EFI thing suitable for booting with QEMU, possibly running
it also.

UEFI binaries for QEMU used for testing this script:
`1
OVMF-pure-efi.i386.fd at
https://drive.google.com/file/d/1jWzOAZfQqMmS2_dAK2G518GhIgj9r2RY/view?usp=sharing

OVMF-pure-efi.x64.fd at
https://drive.google.com/file/d/1c39YI9QtpByGQ4V0UNNQtGqttEzS-eFV/view?usp=sharing

Use ~/.build-efi to configure the various paths used by this script.
"""

from argparse import ArgumentParser
import configparser
import glob
import os
import re
import shutil
import sys
import time


OUR_PATH = os.path.dirname(os.path.realpath(__file__))
OUR1_PATH = os.path.dirname(OUR_PATH)

# Bring in the patman and dtoc libraries (but don't override the first path
# in PYTHONPATH)
sys.path.insert(2, os.path.join(OUR1_PATH, 'tools'))


# pylint: disable=C0413
from u_boot_pylib import command
from u_boot_pylib import tools


def parse_args():
    """Parse the program arguments

    Return:
        Namespace object
    """
    parser = ArgumentParser(
        epilog='Script for running U-Boot as an EFI app/payload')
    parser.add_argument('-a', '--app', action='store_true',
                        help='Package up the app')
    parser.add_argument('-A', '--arm', action='store_true',
                        help='Run on ARM architecture')
    parser.add_argument('-g', '--gnuefi', action='store_true',
                        help='Add gnuefi app')
    parser.add_argument('-k', '--kernel', action='store_true',
                        help='Add a kernel')
    parser.add_argument('-o', '--old', action='store_true',
                        help='Use old EFI app build (before 32/64 split)')
    parser.add_argument('-p', '--payload', action='store_true',
                        help='Package up the payload')
    parser.add_argument('-P', '--partition', action='store_true',
                        help='Create a partition table')
    parser.add_argument('-r', '--run', action='store_true',
                        help='Run QEMU with the image')
    parser.add_argument('-s', '--serial', action='store_true',
                        help='Run QEMU with serial only (no display)')
    parser.add_argument('-t', '--out_app', action='store_true',
                        help='Add try.c out app')
    parser.add_argument('-w', '--word', action='store_true',
                        help='Use word version (32-bit) rather than 64-bit')
    parser.add_argument('-x', '--x1', action='store_true',
                        help='Use x1n version')

    args = parser.parse_args()

    if args.app and args.payload:
        raise ValueError('Please choose either app or payload, not both')
    return args


def get_settings():
    """Get settings from the settings file

    Return:
        ConfigParser containing settings
    """
    settings = configparser.ConfigParser()
    fname = f'{os.getenv("HOME")}/.build-efi'
    if not os.path.exists(fname):
        print('No config file found ~/.build-efi\nCreating one...\n')
        tools.write_file(fname, '''[build-efi]
# Mount path for the temporary image
mount_point = /mnt/test-efi

# Image-output filename
image_file = try.img

# Set ubdir to the build directory where you build U-Boot out-of-tree
# We avoid in-tree build because it gets confusing trying different builds
build_dir = /tmp/b

# Build the kernel with: make O=/tmp/kernel
bzimage = /tmp/kernel/arch/x86/boot/bzImage

# Place where OVMF-pure-efi.i386.fd etc. are kept
efi_dir = .
''', binary=False)
    settings.read(fname)
    return settings


class BuildEfi:
    """Class to collect together the various bits of state while running"""
    def __init__(self, settings, args):
        self.settings = settings
        self.img = self.get_setting('image_file', 'try.img')
        self.build_dir = self.get_setting("build_dir", '/tmp')
        self.mnt = self.get_setting("mount_point", '/mnt/test-efi')
        self.tmp = None
        self.args = args

    def get_setting(self, name, fallback=None):
        """Get a setting by name

        Args:
            name (str): Name of setting to retrieve
            fallback (str or None): Value to return if the setting is missing
        """
        return self.settings.get('build-efi', name, fallback=fallback)

    def run_qemu(self, bitness, serial_only):
        """Run QEMU

        Args:
            bitness (int): Bitness to use, 32 or 64
            serial_only (bool): True to run without a display
        """
        extra = []
        efi_dir = self.get_setting("efi_dir")
        if bitness == 64:
            if self.args.arm :
                qemu_arch = 'aarch64'
                extra += ['--machine', 'virt', '-cpu', 'max']
                # bios = 'OVMG-efi.aarch64.fd'
                # bios = 'QEMU_EFI.fd'
                bios = os.path.join(efi_dir, 'efi.img')
                var_store = os.path.join(efi_dir, 'varstore.img')
                extra += [
                    '-drive', f'if=pflash,format=raw,file={bios},readonly=on',
                    '-drive', f'if=pflash,format=raw,file={var_store}'
                    ]
                extra += ['-drive',
                          f'id=hd0,file={self.img},if=none,format=raw',
                          '-device', 'virtio-blk-device,drive=hd0']
            else:
                qemu_arch = 'x86_64'
                bios = 'OVMF-pure-efi.x64.fd'
                extra += ['-bios', os.path.join(efi_dir, bios)]
                extra += ['-drive', f'id=disk,file={self.img},if=none,format=raw']
                extra += ['-device', 'ahci,id=ahci']
                extra += ['-device', 'ide-hd,drive=disk,bus=ahci.0']
        else:
            qemu_arch = 'arm' if self.args.arm else 'i386'
            bios = 'OVMF-pure-efi.i386.fd'
        qemu = f'qemu-system-{qemu_arch}'
        if serial_only:
            extra += ['-display', 'none', '-serial', 'mon:stdio']
            serial_msg = ' (Ctrl-a x to quit)'
        else:
            extra += ['-serial', 'mon:stdio']
            serial_msg = ''
        print(f'Running {qemu}{serial_msg}')

        # Use 512MB since U-Boot EFI likes to have 256MB to play with
        cmd = [qemu]
        cmd += '-m', '2048'
        cmd += '-nic', 'none'
        cmd += extra
        command.run(*cmd)

    def setup_files(self, build, build_type):
        """Set up files in the staging area

        Args:
            build (str): Name of build being packaged, e.g. 'efi-x86_app32'
            build_type (str): Build type ('app' or 'payload')
        """
        print(f'Packaging {build}')
        if not os.path.exists(self.tmp):
            os.mkdir(self.tmp)
        fname = f'u-boot-{build_type}.efi'
        if self.args.x1:
            fname = 'u-boot-payload.efi'
        elif self.args.gnuefi:
            fname = 'printenv.efi'
        elif self.args.out_app:
            fname = 'out.efi'
        tools.write_file(f'{self.tmp}/startup.nsh', f'fs0:{fname}',
                         binary=False)
        if self.args.gnuefi:
            shutil.copy(fname, self.tmp)
        elif self.args.out_app:
            shutil.copy(fname, self.tmp)
        elif self.args.x1:
            shutil.copy(f'/tmp/b/x1e/{fname}', self.tmp)
        else:
            shutil.copy(f'{self.build_dir}/{build}/{fname}', self.tmp)

    def copy_files(self):
        """Copy files into the filesystem"""
        command.run('sudo', 'cp', *glob.glob(f'{self.tmp}/*'), self.mnt)
        if self.args.kernel:
            bzimage = self.get_setting('bzimage_file', 'bzImage')
            command.run('sudo', 'cp', bzimage, f'{self.mnt}/vmlinuz')

    def setup_raw(self):
        """Create a filesystem on a raw device and copy in the files"""
        command.output('mkfs.vfat', self.img)
        command.run('sudo', 'mount', '-o', 'loop', self.img, self.mnt)
        self.copy_files()
        command.run('sudo', 'umount', self.mnt)

    def setup_part(self):
        """Set up a partition table

        Create a partition table and put the filesystem in the first partition
        then copy in the files
        """

        # Create a gpt partition table with one partition
        command.run('parted', self.img, 'mklabel', 'gpt', capture_stderr=True)

        # This doesn't work correctly. It creates:
        # Number  Start   End     Size    File system  Name  Flags
        #  1      1049kB  24.1MB  23.1MB               boot  msftdata
        # Odd if the same is entered interactively it does set the FS type
        command.run('parted', '-s', '-a', 'optimal', '--',
                    self.img, 'mkpart', 'boot', 'fat32', '1MiB', '23MiB')

        # Map this partition to a loop device. Output is something like:
        # add map loop48p1 (252:3): 0 45056 linear 7:48 2048
        out = command.output('sudo', 'kpartx', '-av', self.img)
        m = re.search(r'(loop.*p.)', out)
        if not m:
            raise ValueError(f'Invalid output from kpartx: {out}')

        boot_dev = m.group(1)
        dev = f'/dev/mapper/{boot_dev}'

        command.output('mkfs.vfat', dev)

        command.run('sudo', 'mount', '-o', 'loop', dev, self.mnt)

        try:
            self.copy_files()
        finally:
            # Sync here since this makes kpartx more likely to work the first time
            command.run('sync')
            command.run('sudo', 'umount', self.mnt)

            # For some reason this needs a sleep or it sometimes fails, if it was
            # run recently (in the last few seconds)
            try:
                cmd = 'sudo', 'kpartx', '-d', self.img
                command.output(*cmd)
            except ValueError:
                time.sleep(0.5)
                command.output(*cmd)

    def do_build(self, build):
        """Build U-Boot for the selected board"""
        res = command.run_one('buildman', '-w', '-o',
                              f'{self.build_dir}/{build}', '--board', build,
                              '-I', raise_on_error=False)
        if res.return_code and res.return_code != 101:  # Allow warnings
            raise ValueError(
                f'buildman exited with {res.return_code}: {res.combined}')

    def start(self):
        """This does all the work"""
        args = self.args
        bitness = 32 if args.word else 64
        arch = 'arm' if args.arm else 'x86'
        build_type = 'payload' if args.payload else 'app'
        self.tmp = f'{self.build_dir}/efi{bitness}{build_type}'
        build = f'efi-{arch}_{build_type}{bitness}'

        self.do_build(build)

        if args.old and bitness == 32:
            build = f'efi-{arch}_{build_type}'

        self.setup_files(build, build_type)

        command.output('qemu-img', 'create', self.img, '24M')

        if args.partition:
            self.setup_part()
        else:
            self.setup_raw()

        if args.run:
            self.run_qemu(bitness, args.serial)


if __name__ == "__main__":
    efi = BuildEfi(get_settings(), parse_args())
    efi.start()
