# SPDX-License-Identifier: GPL-2.0
# Copyright 2021 Google LLC

import pytest

@pytest.mark.buildconfigspec('target_qemu_arm_32bit_spl')
def test_passage(u_boot_console):
    """Test that the standard passage on ARM from SPL to U-Boot works."""

    response = u_boot_console.run_command('bdinfo')
    assert 'devicetree  = passage' in response
