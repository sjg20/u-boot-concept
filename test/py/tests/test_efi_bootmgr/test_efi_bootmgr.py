# SPDX-License-Identifier:      GPL-2.0+
""" Unit test for UEFI bootmanager
"""

import pytest

@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec('cmd_efidebug')
@pytest.mark.buildconfigspec('cmd_bootefi_bootmgr')
@pytest.mark.singlethread
def test_efi_bootmgr(ubpy, efi_bootmgr_data):
    """ Unit test for UEFI bootmanager
    The efidebug command is used to set up UEFI load options.
    The bootefi bootmgr loads initrddump.efi as a payload.
    The crc32 of the loaded initrd.img is checked

    Args:
        ubpy -- U-Boot console
        efi_bootmgr_data -- Path to the disk image used for testing.
    """
    ubpy.run_command(cmd = f'host bind 0 {efi_bootmgr_data}')

    ubpy.run_command(cmd = 'efidebug boot add ' \
        '-b 0001 label-1 host 0:1 initrddump.efi ' \
        '-i host 0:1 initrd-1.img -s nocolor')
    ubpy.run_command(cmd = 'efidebug boot dump')
    ubpy.run_command(cmd = 'efidebug boot order 0001')
    ubpy.run_command(cmd = 'bootefi bootmgr')
    response = ubpy.run_command(cmd = 'load', wait_for_echo=False)
    assert 'crc32: 0x181464af' in response
    ubpy.run_command(cmd = 'exit', wait_for_echo=False)

    ubpy.run_command(cmd = 'efidebug boot add ' \
        '-B 0002 label-2 host 0:1 initrddump.efi ' \
        '-I host 0:1 initrd-2.img -s nocolor')
    ubpy.run_command(cmd = 'efidebug boot dump')
    ubpy.run_command(cmd = 'efidebug boot order 0002')
    ubpy.run_command(cmd = 'bootefi bootmgr')
    response = ubpy.run_command(cmd = 'load', wait_for_echo=False)
    assert 'crc32: 0x811d3515' in response
    ubpy.run_command(cmd = 'exit', wait_for_echo=False)

    ubpy.run_command(cmd = 'efidebug boot rm 0001')
    ubpy.run_command(cmd = 'efidebug boot rm 0002')
