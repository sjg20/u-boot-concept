# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2015 Stephen Warren
# Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.

import pytest

def test_help(ubpy):
    """Test that the "help" command can be executed."""

    lines = ubpy.run_command('help')
    if ubpy.config.buildconfig.get('config_cmd_2048', 'n') == 'y':
        assert lines.splitlines()[0] == "2048      - The 2048 game"
    else:
        assert lines.splitlines()[0] == "?         - alias for 'help'"

@pytest.mark.boardspec('sandbox')
def test_help_no_devicetree(ubpy):
    try:
        cons = ubpy
        cons.restart_uboot_with_flags([], use_dtb=False)
        cons.run_command('help')
        output = cons.get_spawn_output().replace('\r', '')
        assert 'print command description/usage' in output
    finally:
        # Restart afterward to get the normal device tree back
        ubpy.restart_uboot()

@pytest.mark.boardspec('sandbox_vpl')
def test_vpl_help(ubpy):
    try:
        cons = ubpy
        cons.restart_uboot()
        cons.run_command('help')
        output = cons.get_spawn_output().replace('\r', '')
        assert 'print command description/usage' in output
    finally:
        # Restart afterward to get the normal device tree back
        ubpy.restart_uboot()
