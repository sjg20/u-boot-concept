# SPDX-License-Identifier:      GPL-2.0+

"""Fixture for cat command test
"""

import os
import shutil
from subprocess import check_call
import pytest

@pytest.fixture(scope='session')
def cat_data(u_boot_config):
    """Set up a file system to be used in cat tests

    Args:
        u_boot_config -- U-boot configuration.

    Return:
        A path to disk image to be used for testing
    """
    mnt_point = u_boot_config.persistent_data_dir + '/test_cat'
    image_path = u_boot_config.persistent_data_dir + '/cat.img'

    shutil.rmtree(mnt_point, ignore_errors=True)
    os.mkdir(mnt_point, mode = 0o755)

    with open(mnt_point + '/hello', 'w', encoding = 'ascii') as file:
        file.write('hello world\n')

    check_call(f'virt-make-fs --partition=gpt --size=+1M --type=vfat {mnt_point} {image_path}',
               shell=True)

    return image_path
