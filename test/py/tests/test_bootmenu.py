# SPDX-License-Identifier: GPL-2.0+

"""Test bootmenu"""

import pytest

@pytest.mark.buildconfigspec('cmd_bootmenu')
def test_bootmenu(ubpy):
    """Test bootmenu

    ubpy -- U-Boot console
    """

    with ubpy.temporary_timeout(500):
        ubpy.run_command('setenv bootmenu_default 1')
        ubpy.run_command('setenv bootmenu_0 test 1=echo ok 1')
        ubpy.run_command('setenv bootmenu_1 test 2=echo ok 2')
        ubpy.run_command('setenv bootmenu_2 test 3=echo ok 3')
        ubpy.run_command('bootmenu 2', wait_for_prompt=False)
        for i in ('U-Boot Boot Menu', 'test 1', 'test 2', 'test 3', 'autoboot'):
            ubpy.p.expect([i])
        # Press enter key to execute default entry
        response = ubpy.run_command(cmd='\x0d', wait_for_echo=False, send_nl=False)
        assert 'ok 2' in response
        ubpy.run_command('bootmenu 2', wait_for_prompt=False)
        ubpy.p.expect(['autoboot'])
        # Press up key to select prior entry followed by the enter key
        response = ubpy.run_command(cmd='\x1b\x5b\x41\x0d', wait_for_echo=False,
                                              send_nl=False)
        assert 'ok 1' in response
        ubpy.run_command('bootmenu 2', wait_for_prompt=False)
        ubpy.p.expect(['autoboot'])
        # Press down key to select next entry followed by the enter key
        response = ubpy.run_command(cmd='\x1b\x5b\x42\x0d', wait_for_echo=False,
                                              send_nl=False)
        assert 'ok 3' in response
        ubpy.run_command('bootmenu 2; echo rc:$?', wait_for_prompt=False)
        ubpy.p.expect(['autoboot'])
        # Press the escape key
        response = ubpy.run_command(cmd='\x1b', wait_for_echo=False, send_nl=False)
        assert 'ok' not in response
        assert 'rc:0' in response
        ubpy.run_command('setenv bootmenu_default')
        ubpy.run_command('setenv bootmenu_0')
        ubpy.run_command('setenv bootmenu_1')
        ubpy.run_command('setenv bootmenu_2')
