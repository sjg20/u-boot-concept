# SPDX-License-Identifier:      GPL-2.0+
""" Unit test for UEFI bootmanager
"""

import pytest

@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec('cmd_efidebug')
@pytest.mark.buildconfigspec('cmd_bootefi_bootmgr')
@pytest.mark.singlethread
def test_efi_bootmgr(u_boot_console, efi_bootmgr_data):
    """ Unit test for UEFI bootmanager
    The efidebug command is used to set up UEFI load options.
    The bootefi bootmgr loads initrddump.efi as a payload.
    The crc32 of the loaded initrd.img is checked

    Args:
        u_boot_console -- U-Boot console
        efi_bootmgr_data -- Path to the disk image used for testing.
    """
    u_boot_console.run_command(cmd = f'host bind 0 {efi_bootmgr_data}')
    output = u_boot_console.run_command('ut cmd -f test_efi_bootmgr_norun')
    assert 'Failures: 0' in output
