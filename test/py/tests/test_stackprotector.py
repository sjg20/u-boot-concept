# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2021 Broadcom

import pytest
import signal

@pytest.mark.buildconfigspec('cmd_stackprotector_test')
@pytest.mark.notbuildconfigspec('asan')
def test_stackprotector(ubpy):
    """Test that the stackprotector function works."""

    ubpy.run_command('stackprot_test',wait_for_prompt=False)
    expected_response = 'Stack smashing detected'
    ubpy.wait_for(expected_response)
    ubpy.restart_uboot()
