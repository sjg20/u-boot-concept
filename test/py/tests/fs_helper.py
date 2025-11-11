# SPDX-License-Identifier:      GPL-2.0+
#
# Copyright (c) 2018, Linaro Limited
# Author: Takahiro Akashi <takahiro.akashi@linaro.org>

"""Helper functions for dealing with filesystems"""

import re
import os
import shutil
from subprocess import call, check_call, check_output, CalledProcessError, run
from subprocess import DEVNULL
import tempfile


class FsHelper:
    """Creating a filesystem containing test files

    Usage:
        with FsHelper(ubman.config, 'ext4', 10, 'mmc1') as fsh:
            # create files in the self.srcdir directory
            fsh.mk_fs()
            # Now use the filesystem

        # The filesystem and srcdir are erased after the 'with' statement.

        To set the image filename:

            with FsHelper(ubman.config, 'ext4', 10, 'mmc1') as fsh:
                self.fs_img = 'myfile.img'
                fsh.mk_fs()
                ...

        It is also possible to use an existing srcdir:

            with FsHelper(ubman.config, 'fat32', 10, 'usb2') as fsh:
                fsh.srcdir = src_dir
                fsh.mk_fs()
                ...

        To create an encrypted LUKS2 partition (default):

            with FsHelper(ubman.config, 'ext4', 10, 'mmc1',
                          passphrase='test') as fsh:
                # create files in the fsh.srcdir directory
                fsh.mk_fs()  # Creates and encrypts the filesystem with LUKS2
                ...

        To create an encrypted LUKS1 partition:

            with FsHelper(ubman.config, 'ext4', 10, 'mmc1',
                          passphrase='test', luks_version=1) as fsh:
                # create files in the fsh.srcdir directory
                fsh.mk_fs()  # Creates and encrypts the filesystem with LUKS1
                ...

        To create an encrypted LUKS2 partition with Argon2id:

            with FsHelper(ubman.config, 'ext4', 10, 'mmc1',
                          passphrase='test', luks_kdf='argon2id') as fsh:
                # create files in the fsh.srcdir directory
                fsh.mk_fs()  # Creates and encrypts the FS with LUKS2+Argon2
                ...

    Properties:
        fs_img (str): Filename for the filesystem image; this is set to a
            default value but can be overwritten
    """
    def __init__(self, config, fs_type, size_mb, prefix, part_mb=None,
                 passphrase=None, luks_version=2, luks_kdf='pbkdf2'):
        """Set up a new object

        Args:
            config (u_boot_config): U-Boot configuration
            fs_type (str): File system type: one of ext2, ext3, ext4, vfat,
                fat12, fat16, fat32, exfat, fs_generic (which means vfat)
            size_mb (int): Size of file system in MB
            prefix (str): Prefix string of volume's file name
            part_mb (int, optional): Size of partition in MB. If None, defaults
                to size_mb. This can be used to make the partition larger than
                the filesystem, to create space for disk-encryption metadata
            passphrase (str, optional): If provided, encrypt the
                filesystem with LUKS using this passphrase
            luks_version (int): LUKS version to use (1 or 2). Defaults to 2.
            luks_kdf (str): Key derivation function for LUKS2: 'pbkdf2' or
                'argon2id'. Defaults to 'pbkdf2'. Ignored for LUKS1.
        """
        if ('fat' not in fs_type and 'ext' not in fs_type and
             fs_type not in ['exfat', 'fs_generic']):
            raise ValueError(f"Unsupported filesystem type '{fs_type}'")

        self.config = config
        self.fs_type = fs_type
        self.size_mb = size_mb
        self.partition_mb = part_mb if part_mb is not None else size_mb
        self.prefix = prefix
        self.quiet = True
        self.passphrase = passphrase
        self.luks_version = luks_version
        self.luks_kdf = luks_kdf

        # Use a default filename; the caller can adjust it
        leaf = f'{prefix}.{fs_type}.img'
        if config:
            self.fs_img = os.path.join(config.persistent_data_dir, leaf)
            if os.path.exists(self.fs_img):
                os.remove(self.fs_img)
        else:
            self.fs_img = leaf

        # Some distributions do not add /sbin to the default PATH, where mkfs
        # lives
        if '/sbin' not in os.environ["PATH"].split(os.pathsep):
            os.environ["PATH"] += os.pathsep + '/sbin'

        self.tmpdir = None
        self.srcdir = None
        self._do_cleanup = False

    def _get_fs_args(self):
        """Get the mkfs options and program to use

        Returns:
            tuple:
                str: mkfs options, e.g. '-F 32' for fat32
                str: mkfs program to use, e.g 'ext4' for ext4
        """
        if self.fs_type == 'fat12':
            mkfs_opt = '-F 12'
        elif self.fs_type == 'fat16':
            mkfs_opt = '-F 16'
        elif self.fs_type == 'fat32':
            mkfs_opt = '-F 32'
        elif self.fs_type.startswith('ext'):
            mkfs_opt = f'-d {self.srcdir}'
        else:
            mkfs_opt = ''

        if self.fs_type == 'exfat':
            fs_lnxtype = 'exfat'
        elif re.match('fat', self.fs_type) or self.fs_type == 'fs_generic':
            fs_lnxtype = 'vfat'
        else:
            fs_lnxtype = self.fs_type
        return mkfs_opt, fs_lnxtype

    def mk_fs(self):
        """Make a new filesystem and copy in the files"""
        self.setup()
        mkfs_opt, fs_lnxtype = self._get_fs_args()
        fs_img = self.fs_img
        with open(fs_img, 'wb') as fsi:
            fsi.truncate(self.size_mb << 20)
        self._do_cleanup = True

        mkfs_opt, fs_lnxtype = self._get_fs_args()
        check_call(f'mkfs.{fs_lnxtype} {mkfs_opt} {fs_img}', shell=True,
                stdout=DEVNULL if self.quiet else None)

        if self.fs_type.startswith('ext'):
            sb_content = check_output(f'tune2fs -l {fs_img}',
                                    shell=True).decode()
            if 'metadata_csum' in sb_content:
                check_call(f'tune2fs -O ^metadata_csum {fs_img}', shell=True)
        elif fs_lnxtype == 'exfat':
            check_call(f'fattools cp {self.srcdir}/* {fs_img}', shell=True)
        elif self.srcdir and os.listdir(self.srcdir):
            flags = f"-smpQ{'' if self.quiet else 'v'}"
            check_call(f'mcopy -i {fs_img} {flags} {self.srcdir}/* ::/',
                    shell=True)

        # Encrypt the filesystem if requested
        if self.passphrase:
            self.encrypt_luks(self.passphrase)

    def setup(self):
        """Set up the srcdir ready to receive files"""
        if not self.srcdir:
            if self.config:
                self.srcdir = os.path.join(self.config.persistent_data_dir,
                                           f'{self.prefix}.{self.fs_type}.tmp')
                if os.path.exists(self.srcdir):
                    shutil.rmtree(self.srcdir)
                os.mkdir(self.srcdir)
            else:
                self.tmpdir = tempfile.TemporaryDirectory('fs_helper')
                self.srcdir = self.tmpdir.name

    def encrypt_luks(self, passphrase):
        """Encrypt the filesystem image with LUKS

        This replaces the filesystem image with a LUKS-encrypted version.
        The LUKS version is determined by self.luks_version.

        Args:
            passphrase (str): Passphrase for the LUKS container

        Returns:
            str: Path to the encrypted image

        Raises:
            CalledProcessError: If cryptsetup is not available or fails
            ValueError: If an unsupported LUKS version is specified
        """
        # LUKS encryption parameters
        if self.luks_version == 1:
            # LUKS1 parameters
            cipher = 'aes-cbc-essiv:sha256'
            key_size = 256
            hash_alg = 'sha256'
            luks_type = 'luks1'
        elif self.luks_version == 2:
            # LUKS2 parameters (modern defaults)
            cipher = 'aes-xts-plain64'
            key_size = 512  # XTS uses 512-bit keys (2x256)
            hash_alg = 'sha256'
            luks_type = 'luks2'
        else:
            raise ValueError(f"Unsupported LUKS version: {self.luks_version}")

        key_size_str = str(key_size)

        # Save the original filesystem image
        orig_fs_img = f'{self.fs_img}.orig'
        os.rename(self.fs_img, orig_fs_img)

        # Create a new image file for the LUKS container
        luks_img = self.fs_img
        luks_size_mb = self.partition_mb
        check_call(f'dd if=/dev/zero of={luks_img} bs=1M count={luks_size_mb}',
                   shell=True, stdout=DEVNULL if self.quiet else None)

        # Ensure device-mapper kernel module is loaded
        if not os.path.exists('/sys/class/misc/device-mapper'):
            # Try to load the dm_mod kernel module
            result = run(['sudo', 'modprobe', 'dm_mod'],
                        stdout=DEVNULL, stderr=DEVNULL, check=False)
            if result.returncode != 0:
                raise RuntimeError(
                    'Device-mapper is not available. Please ensure the dm_mod '
                    'kernel module is loaded and you have permission to use '
                    'device-mapper. This is required for LUKS encryption tests.')

        device_name = f'luks_test_{os.getpid()}'

        # Clean up any stale device with the same name
        run(['sudo', 'cryptsetup', 'close', device_name],
            stdout=DEVNULL, stderr=DEVNULL, check=False)

        try:
            # Format as LUKS (version determined by luks_type)
            cmd = ['cryptsetup', 'luksFormat',
                   '--type', luks_type,
                   '--cipher', cipher,
                   '--key-size', key_size_str,
                   '--hash', hash_alg,
                   '--iter-time', '10']  # Very fast for testing (low security)

            # For LUKS2, specify the KDF (pbkdf2 or argon2id)
            if self.luks_version == 2:
                cmd.extend(['--pbkdf', self.luks_kdf])
                # For Argon2, use low memory/time settings suitable for testing
                if self.luks_kdf == 'argon2id':
                    cmd.extend([
                        '--pbkdf-memory', '65536',  # 64MB
                        '--pbkdf-parallel', '4',
                    ])

            cmd.append(luks_img)

            run(cmd,
                input=f'{passphrase}\n'.encode(),
                stdout=DEVNULL if self.quiet else None,
                stderr=DEVNULL if self.quiet else None,
                check=True)

            # Open the LUKS device (requires sudo)
            # Use --key-file=- to read passphrase from stdin
            result = run(['sudo', 'cryptsetup', 'open', '--key-file=-',
                          luks_img, device_name], input=passphrase.encode(),
                          stdout=DEVNULL if self.quiet else None, stderr=None,
                          check=True)
            # Copy the filesystem data into the LUKS container
            check_call(f'sudo dd if={orig_fs_img} of=/dev/mapper/{device_name} bs=1M',
                        shell=True, stdout=DEVNULL if self.quiet else None)

            # Remove the original filesystem image
            os.remove(orig_fs_img)

        except Exception:
            # Clean up on error
            if os.path.exists(luks_img):
                os.remove(luks_img)
            if os.path.exists(orig_fs_img):
                os.rename(orig_fs_img, self.fs_img)
            raise
        finally:
            # Always close the device if it's still open
            run(['sudo', 'cryptsetup', 'close', device_name],
                stdout=DEVNULL, stderr=DEVNULL, check=False)

        return self.fs_img

    def cleanup(self):
        """Remove created image"""
        if self.tmpdir:
            self.tmpdir.cleanup()
        if self._do_cleanup:
            os.remove(self.fs_img)

    def __enter__(self):
        self.setup()
        return self

    def __exit__(self, extype, value, traceback):
        self.cleanup()


class DiskHelper:
    """Helper class for creating disk images containing filesytems

    Usage:
        with DiskHelper(ubman.config, 0, 'mmc') as img, \
                FsHelper(ubman.config, 'ext1', 1, 'mmc') as fsh:
            # Write files to fsh.srcdir
            ...

            # Create the filesystem
            fsh.mk_fs()

            # Add this filesystem to the disk
            img.add_fs(fsh, DiskHelper.VFAT)

            # Add more filesystems as needed (add another 'with' clause)
            ...

            # Get the final disk image
            data = img.create()
    """

    # Partition-type codes
    VFAT  = 0xc
    EXT4  = 0x83

    def __init__(self, config, devnum, prefix, cur_dir=False):
        """Set up a new disk image

        Args:
            config (u_boot_config): U-Boot configuration
            devnum (int): Device number (for filename)
            prefix (str): Prefix string of volume's file name
            cur_dir (bool): True to put the file in the current directory,
                instead of the persistent-data directory
        """
        self.fs_list = []
        self.fname = os.path.join('' if cur_dir else config.persistent_data_dir,
                                  f'{prefix}{devnum}.img')
        self.remove_img = True
        self._do_cleanup = False

    def add_fs(self, fs_img, part_type, bootable=False):
        """Add a new filesystem

        Args:
            fs_img (FsHelper): Filesystem to add
            part_type (DiskHelper.FAT or DiskHelper.EXT4): Partition type
            bootable (bool): True to set the 'bootable' flat
        """
        self.fs_list.append([fs_img, part_type, bootable])

    def create(self):
        """Create the disk image

        Create an image with a partition table and the filesystems
        """
        spec = ''
        pos = 1   # Reserve 1MB for the partition table itself
        for fsi, part_type, bootable in self.fs_list:
            if spec:
                spec += '\n'
            spec += f'type={part_type:x}, size={fsi.partition_mb}M, start={pos}M'
            if bootable:
                spec += ', bootable'
            pos += fsi.partition_mb

        img_size = pos
        try:
            check_call(f'qemu-img create {self.fname} {img_size}M', shell=True)
            self._do_cleanup = True
            check_call(f'printf "{spec}" | sfdisk {self.fname}', shell=True)
        except CalledProcessError:
            os.remove(self.fname)
            raise

        pos = 1   # Reserve 1MB for the partition table itself
        for fsi, part_type, bootable in self.fs_list:
            check_call(
                f'dd if={fsi.fs_img} of={self.fname} bs=1M seek={pos} conv=notrunc',
                shell=True)
            pos += fsi.partition_mb
        return self.fname

    def cleanup(self):
        """Remove created file"""
        if self._do_cleanup:
            os.remove(self.fname)

    def __enter__(self):
        return self

    def __exit__(self, extype, value, traceback):
        if self.remove_img:
            self.cleanup()


def mk_fs(config, fs_type, size, prefix, src_dir=None, size_gran = 0x100000,
          fs_img=None, quiet=False):
    """Create a file system volume

    Args:
        config (u_boot_config): U-Boot configuration
        fs_type (str): File system type, e.g. 'ext4'
        size (int): Size of file system in bytes
        prefix (str): Prefix string of volume's file name
        src_dir (str): Root directory to use, or None for none
        fs_img (str or None): Filename for image, or None to invent one
        quiet (bool): Suppress non-error output

    Raises:
        CalledProcessError: if any error occurs when creating the filesystem
    """
    fsh = FsHelper(config, fs_type, size >> 20, prefix)
    fsh.srcdir = src_dir
    if fs_img:
        fsh.fs_img = fs_img
    fsh.mk_fs()
    return fsh.fs_img


def setup_image(ubman, devnum, part_type, img_size=20, second_part=False,
                basename='mmc'):
    """Create a disk image with one or two partitions

    Args:
        ubman (ConsoleBase): Console to use
        devnum (int): Device number to use, e.g. 1
        part_type (int): Partition type, e.g. 0xc for FAT32
        img_size (int): Image size in MiB
        second_part (bool): True to contain a small second partition
        basename (str): Base name to use in the filename, e.g. 'mmc'

    Returns:
        tuple:
            str: Filename of MMC image
            str: Directory name of scratch directory
    """
    fname = os.path.join(ubman.config.source_dir, f'{basename}{devnum}.img')
    mnt = os.path.join(ubman.config.persistent_data_dir, 'scratch')

    spec = f'type={part_type:x}, size={img_size - 2}M, start=1M, bootable'
    if second_part:
        spec += '\ntype=c'

    try:
        check_call(f'mkdir -p {mnt}', shell=True)
        check_call(f'qemu-img create {fname} {img_size}M', shell=True)
        check_call(f'printf "{spec}" | sfdisk {fname}', shell=True)
    except CalledProcessError:
        call(f'rm -f {fname}', shell=True)
        raise

    return fname, mnt

# Just for trying out
if __name__ == "__main__":
    import collections

    CNF= collections.namedtuple('config', 'persistent_data_dir')

    mk_fs(CNF('.'), 'ext4', 0x1000000, 'pref')
