# Copyright (c) 2015 Stephen Warren
#
# SPDX-License-Identifier: GPL-2.0

def test_unknown_command(uboot_console):
    with uboot_console.disable_check("unknown_command"):
        response = uboot_console.run_command("non_existent_cmd")
    assert("Unknown command 'non_existent_cmd' - try 'help'" in response)
