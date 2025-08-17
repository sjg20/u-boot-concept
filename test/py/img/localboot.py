# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.

"""Create localboot test disk images"""

from img.common import setup_extlinux_image


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