# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.

"""Create configuration editor test files"""

import os

import utils


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