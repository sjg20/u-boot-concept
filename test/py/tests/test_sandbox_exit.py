# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2015 Stephen Warren
# Copyright (c) 2015-2016, NVIDIA CORPORATION. All rights reserved.

import pytest
import signal

@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec('sysreset_cmd_poweroff')
def test_poweroff(ubpy):
    """Test that the "poweroff" command exits sandbox process."""

    ubpy.run_command('poweroff', wait_for_prompt=False)
    assert(ubpy.validate_exited())

@pytest.mark.boardspec('sandbox')
def test_ctrl_c(ubpy):
    """Test that sending SIGINT to sandbox causes it to exit."""

    ubpy.kill(signal.SIGINT)
    assert(ubpy.validate_exited())

@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec('cmd_exception')
@pytest.mark.buildconfigspec('sandbox_crash_reset')
def test_exception_reset(ubpy):
    """Test that SIGILL causes a reset."""

    ubpy.run_command('exception undefined', wait_for_prompt=False)
    m = ubpy.p.expect(['resetting ...', 'U-Boot'])
    if m != 0:
        raise Exception('SIGILL did not lead to reset')
    m = ubpy.p.expect(['U-Boot', '=>'])
    if m != 0:
        raise Exception('SIGILL did not lead to reset')
    ubpy.restart_uboot()

@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec('cmd_exception')
@pytest.mark.notbuildconfigspec('sandbox_crash_reset')
def test_exception_exit(ubpy):
    """Test that SIGILL causes a reset."""

    ubpy.run_command('exception undefined', wait_for_prompt=False)
    assert(ubpy.validate_exited())
