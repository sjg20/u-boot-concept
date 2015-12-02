# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# SPDX-License-Identifier: GPL-2.0

import os
import os.path
import pytest

subtests = (
    # Base if functionality

    ("true", True),
    ("false", False),

    # Basic operators

    ("test aaa = aaa", True),
    ("test aaa = bbb", False),

    ("test aaa != bbb", True),
    ("test aaa != aaa", False),

    ("test aaa < bbb", True),
    ("test bbb < aaa", False),

    ("test bbb > aaa", True),
    ("test aaa > bbb", False),

    ("test 123 -eq 123", True),
    ("test 123 -eq 456", False),

    ("test 123 -ne 456", True),
    ("test 123 -ne 123", False),

    ("test 123 -lt 456", True),
    ("test 123 -lt 123", False),
    ("test 456 -lt 123", False),

    ("test 123 -le 456", True),
    ("test 123 -le 123", True),
    ("test 456 -le 123", False),

    ("test 456 -gt 123", True),
    ("test 123 -gt 123", False),
    ("test 123 -gt 456", False),

    ("test 456 -ge 123", True),
    ("test 123 -ge 123", True),
    ("test 123 -ge 456", False),

    ("test -z \"\"", True),
    ("test -z \"aaa\"", False),

    ("test -n \"aaa\"", True),
    ("test -n \"\"", False),

    # Inversion of simple tests

    ("test ! aaa = aaa", False),
    ("test ! aaa = bbb", True),
    ("test ! ! aaa = aaa", True),
    ("test ! ! aaa = bbb", False),

    # Binary operators

    ("test aaa != aaa -o bbb != bbb", False),
    ("test aaa != aaa -o bbb = bbb", True),
    ("test aaa = aaa -o bbb != bbb", True),
    ("test aaa = aaa -o bbb = bbb", True),

    ("test aaa != aaa -a bbb != bbb", False),
    ("test aaa != aaa -a bbb = bbb", False),
    ("test aaa = aaa -a bbb != bbb", False),
    ("test aaa = aaa -a bbb = bbb", True),

    # Inversion within binary operators

    ("test ! aaa != aaa -o ! bbb != bbb", True),
    ("test ! aaa != aaa -o ! bbb = bbb", True),
    ("test ! aaa = aaa -o ! bbb != bbb", True),
    ("test ! aaa = aaa -o ! bbb = bbb", False),

    ("test ! ! aaa != aaa -o ! ! bbb != bbb", False),
    ("test ! ! aaa != aaa -o ! ! bbb = bbb", True),
    ("test ! ! aaa = aaa -o ! ! bbb != bbb", True),
    ("test ! ! aaa = aaa -o ! ! bbb = bbb", True),

    # -z operator

    ("test -z \"$ut_var_nonexistent\"", True),
    ("test -z \"$ut_var_exists\"", False),
)

def exec_hush_if(uboot_console, expr, result):
    cmd = "if " + expr + "; then echo true; else echo false; fi"
    response = uboot_console.run_command(cmd)
    assert response.strip() == str(result).lower()

@pytest.mark.buildconfigspec("sys_hush_parser")
def test_hush_if_test_setup(uboot_console):
    uboot_console.run_command("setenv ut_var_nonexistent")
    uboot_console.run_command("setenv ut_var_exists 1")

@pytest.mark.buildconfigspec("sys_hush_parser")
@pytest.mark.parametrize("expr,result", subtests)
def test_hush_if_test(uboot_console, expr, result):
    exec_hush_if(uboot_console, expr, result)

@pytest.mark.buildconfigspec("sys_hush_parser")
def test_hush_if_test_teardown(uboot_console):
    uboot_console.run_command("setenv ut_var_exists")

@pytest.mark.buildconfigspec("sys_hush_parser")
# We might test this on real filesystems via UMS, DFU, "save", etc.
# Of those, only UMS currently allows file removal though.
@pytest.mark.boardspec("sandbox")
def test_hush_if_test_host_file_exists(uboot_console):
    test_file = uboot_console.config.result_dir + \
        "/creating_this_file_breaks_uboot_tests"

    try:
        os.unlink(test_file)
    except:
        pass
    assert not os.path.exists(test_file)

    expr = "test -e hostfs - " + test_file
    exec_hush_if(uboot_console, expr, False)

    try:
        with file(test_file, "wb"):
            pass
        assert os.path.exists(test_file)

        expr = "test -e hostfs - " + test_file
        exec_hush_if(uboot_console, expr, True)
    finally:
        os.unlink(test_file)

    expr = "test -e hostfs - " + test_file
    exec_hush_if(uboot_console, expr, False)
