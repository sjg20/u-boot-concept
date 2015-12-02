# Copyright (c) 2015 Stephen Warren
# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# SPDX-License-Identifier: GPL-2.0

import pytest

@pytest.mark.buildconfigspec("cmd_memory")
def test_md(uboot_console):
    ram_base = uboot_console.find_ram_base()
    addr = "%08x" % ram_base
    val = "a5f09876"
    expected_response = addr + ": " + val
    uboot_console.run_command("mw " + addr + " 0 10")
    response = uboot_console.run_command("md " + addr + " 10")
    assert(not (expected_response in response))
    uboot_console.run_command("mw " + addr + " " + val)
    response = uboot_console.run_command("md " + addr + " 10")
    assert(expected_response in response)

@pytest.mark.buildconfigspec("cmd_memory")
def test_md_repeat(uboot_console):
    ram_base = uboot_console.find_ram_base()
    addr_base = "%08x" % ram_base
    words = 0x10
    addr_repeat = "%08x" % (ram_base + (words * 4))
    uboot_console.run_command("md %s %x" % (addr_base, words))
    response = uboot_console.run_command("")
    expected_response = addr_repeat + ": "
    assert(expected_response in response)
