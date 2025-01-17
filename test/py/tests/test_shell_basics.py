# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2015-2016, NVIDIA CORPORATION. All rights reserved.

# Test basic shell functionality, such as commands separate by semi-colons.

import pytest

pytestmark = pytest.mark.buildconfigspec('cmd_echo')

def test_shell_execute(ubpy):
    """Test any shell command."""

    response = ubpy.run_command('echo hello')
    assert response.strip() == 'hello'

def test_shell_semicolon_two(ubpy):
    """Test two shell commands separate by a semi-colon."""

    cmd = 'echo hello; echo world'
    response = ubpy.run_command(cmd)
    # This validation method ignores the exact whitespace between the strings
    assert response.index('hello') < response.index('world')

def test_shell_semicolon_three(ubpy):
    """Test three shell commands separate by a semi-colon, with variable
    expansion dependencies between them."""

    cmd = 'setenv list 1; setenv list ${list}2; setenv list ${list}3; ' + \
        'echo ${list}'
    response = ubpy.run_command(cmd)
    assert response.strip() == '123'
    ubpy.run_command('setenv list')

def test_shell_run(ubpy):
    """Test the "run" shell command."""

    ubpy.run_command('setenv foo \'setenv monty 1; setenv python 2\'')
    ubpy.run_command('run foo')
    response = ubpy.run_command('echo ${monty}')
    assert response.strip() == '1'
    response = ubpy.run_command('echo ${python}')
    assert response.strip() == '2'
    ubpy.run_command('setenv foo')
    ubpy.run_command('setenv monty')
    ubpy.run_command('setenv python')
