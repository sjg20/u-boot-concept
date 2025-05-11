# SPDX-License-Identifier: GPL-2.0+
#
"""Common script for build- scripts

"""

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
