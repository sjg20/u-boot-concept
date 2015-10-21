# Copyright (c) 2015 Stephen Warren
#
# SPDX-License-Identifier: GPL-2.0

def test_help(uboot_console):
    uboot_console.run_command("help")
