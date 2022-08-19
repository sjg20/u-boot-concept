# SPDX-License-Identifier:      GPL-2.0+

""" Unit test for cat command
"""

import pytest

@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec('cmd_cat')
def test_cat(u_boot_console, cat_data):
    """ Unit test for cat

    Args:
        u_boot_console -- U-Boot console
        cat_data -- Path to the disk image used for testing.
    """
    u_boot_console.run_command(cmd = f'host bind 0 {cat_data}')

    response = u_boot_console.run_command(cmd = 'cat host 0 hello')
    assert 'hello world' == response

    u_boot_console.run_command(cmd = 'exit', wait_for_echo=False)
