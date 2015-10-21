# Copyright (c) 2015 Stephen Warren
#
# SPDX-License-Identifier: GPL-2.0

# pytest runs tests the order of their module path, which is related to the
# filename containing the test. This file is named such that it is sorted
# first, simply as a very basic sanity check of the functionality of the U-Boot
# command prompt.

def test_version(uboot_console):
    with uboot_console.disable_check("main_signon"):
        response = uboot_console.run_command("version")
    uboot_console.validate_main_signon_in_text(response)
