# Copyright (c) 2015 Stephen Warren
# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# SPDX-License-Identifier: GPL-2.0

import pytest
import signal

@pytest.mark.boardspec("sandbox")
@pytest.mark.buildconfigspec("reset")
def test_reset(uboot_console):
    uboot_console.run_command("reset", wait_for_prompt=False)
    assert(uboot_console.validate_exited())
    uboot_console.ensure_spawned()

@pytest.mark.boardspec("sandbox")
def test_ctrlc(uboot_console):
    uboot_console.kill(signal.SIGINT)
    assert(uboot_console.validate_exited())
    uboot_console.ensure_spawned()
