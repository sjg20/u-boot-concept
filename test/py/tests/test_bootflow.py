# SPDX-License-Identifier: GPL-2.0
# Copyright 2021 Google LLC
# Written by Simon Glass <sjg@chromium.org>

import os
import pytest
import u_boot_utils

@pytest.fixture(scope='function')
def bootflow_image(u_boot_console):
    cons = u_boot_console
    fname = os.path.join(cons.config.persistent_data_dir, 'mmc.img')
    mnt = os.path.join(cons.config.persistent_data_dir, 'mnt')
    if not os.path.exists(mnt):
        os.mkdir(mnt)

    u_boot_utils.run_and_log(cons, 'qemu-img create %s 20M' % fname)
    u_boot_utils.run_and_log(cons, 'sudo sfdisk %s' % fname,
                             stdin=b'type=c')

    loop = None
    mounted = False
    try:
        out = u_boot_utils.run_and_log(cons,
                                       'sudo losetup --show -f -P %s' % fname)
        loop = out.strip()
        fatpart = '%sp1' % loop
        u_boot_utils.run_and_log(cons, 'sudo mkfs.vfat %s' % fatpart)
        u_boot_utils.run_and_log(cons, 'sudo mount -o loop %s %s' %
                                 (fatpart, mnt))
        mounted = True
    except:
        if mounted:
            u_boot_utils.run_and_log(cons, 'sudo umount %s' % mnt)
        if loop:
            u_boot_utils.run_and_log(cons, 'sudo losetup -d %s' % loop)
        raise
    yield mnt

    u_boot_utils.run_and_log(cons, 'sudo umount %s' % mnt)
    u_boot_utils.run_and_log(cons, 'sudo losetup -d %s' % loop)


#@pytest.fixture(scope='function')
def test_help(bootflow_image, u_boot_console):
    """Test that the "help" command can be executed."""
    cons = u_boot_console
    cons.run_command('echo fred')
