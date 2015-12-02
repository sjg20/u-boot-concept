# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# SPDX-License-Identifier: GPL-2.0

def test_shell_execute(uboot_console):
    """Test any shell command"""
    response = uboot_console.run_command("echo hello")
    assert response.strip() == "hello"

def test_shell_semicolon_two(uboot_console):
    """Test two shell commands separate by a semi-colon"""
    cmd = "echo hello; echo world"
    response = uboot_console.run_command(cmd)
    # This validation method ignores the exact whitespace between the strings
    assert response.index("hello") < response.index("world")

def test_shell_semicolon_three(uboot_console):
    """Test three shell commands separate by a semi-colon"""
    cmd = "setenv list 1; setenv list ${list}2; setenv list ${list}3; " + \
        "echo ${list}"
    response = uboot_console.run_command(cmd)
    assert response.strip() == "123"
    uboot_console.run_command("setenv list")

def test_shell_run(uboot_console):
    """Test the 'run' shell command"""
    uboot_console.run_command("setenv foo 'setenv monty 1; setenv python 2'")
    uboot_console.run_command("run foo")
    response = uboot_console.run_command("echo $monty")
    assert response.strip() == "1"
    response = uboot_console.run_command("echo $python")
    assert response.strip() == "2"
    uboot_console.run_command("setenv foo")
    uboot_console.run_command("setenv monty")
    uboot_console.run_command("setenv python")
