# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.

import pytest
import u_boot_utils

@pytest.mark.buildconfigspec('cmd_fru')
def test_fru_board(u_boot_console):
    """Test that fru command generates and captures board FRU information as
    expected."""

    ram_base = u_boot_utils.find_ram_base(u_boot_console)
    addr = '0x%x' % ram_base
    expected_response = 'rc:0'
    response = u_boot_console.run_command('fru generate -b ' + addr + ' abcd efgh ijkl mnop qrst uvwx; echo rc:$?')
    assert(expected_response in response)
    response = u_boot_console.run_command('fru capture ' + addr + '; echo rc:$?')
    assert(expected_response in response)
    response = u_boot_console.run_command('fru display')
    assert('Manufacturer Name: abcd' in response)
    assert('Product Name: efgh' in response)
    assert('Serial Number: ijkl' in response)
    assert('Part Number: mnop' in response)
    assert('File ID: qrst' in response)
    assert('Custom Type/Length: 0xc4' in response)

@pytest.mark.buildconfigspec('cmd_fru')
def test_fru_product(u_boot_console):
    """Test that fru command generates and captures product FRU information as
    expected."""

    ram_base = u_boot_utils.find_ram_base(u_boot_console)
    addr = '0x%x' % ram_base
    expected_response = 'rc:0'
    response = u_boot_console.run_command('fru generate -p ' + addr + ' abcd efgh ijkl mnop qrst uvwx yz01 2345; echo rc:$?')
    assert(expected_response in response)
    response = u_boot_console.run_command('fru capture ' + addr + '; echo rc:$?')
    assert(expected_response in response)
    response = u_boot_console.run_command('fru display')
    assert('Manufacturer Name: abcd' in response)
    assert('Product Name: efgh' in response)
    assert('Part Number: ijkl' in response)
    assert('Version Number: mnop' in response)
    assert('Serial Number: qrst' in response)
    assert('Asset Number: uvwx' in response)
    assert('File ID: yz01' in response)
    assert('Custom Type/Length: 0xc4' in response)
