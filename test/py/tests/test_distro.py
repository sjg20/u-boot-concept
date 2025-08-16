# SPDX-License-Identifier: GPL-2.0+
# Copyright 2025 Canonical Ltd.
# Written by Simon Glass <simon.glass@canonical.com>

import pytest
import utils

# Enable early console so that the test can see if something goes wrong
CONSOLE = 'earlycon=uart8250,io,0x3f8 console=uart8250,io,0x3f8'

@pytest.mark.boardspec('qemu-x86_64')
@pytest.mark.role('qemu-x86_64')
def test_distro(ubman):
    """Test booting into Ubuntu 24.04"""
    with ubman.log.section('boot'):
        ubman.run_command('boot', wait_for_prompt=False)

    with ubman.log.section('Grub'):
        # Wait for grub to come up and offset a menu
        ubman.expect(['Try or Install Ubuntu'])

        # Press 'e' to edit the command line
        ubman.log.info("Pressing 'e'")
        ubman.run_command('e', wait_for_prompt=False, send_nl=False)

        # Wait until we see the editor appear
        ubman.expect(['/casper/initrd'])

        # Go down to the 'linux' line. Avoid using down-arrow as that includes
        # an Escape character, which may be parsed by Grub as such, causing it
        # to return to the top menu
        ubman.log.info("Going DOWN")
        ubman.ctrl('N')
        ubman.ctrl('N')
        ubman.ctrl('N')

        # Go to end of line
        ubman.log.info("Going to EOL")
        ubman.ctrl('E')

        # Backspace to remove 'quiet splash'
        ubman.log.info("Erasing quiet and splash")
        ubman.send('\b' * len('quiet splash'))

        # Send our noisy console
        ubman.log.info("Noisy console")
        ubman.send(CONSOLE)

        # Tell grub to boot
        ubman.log.info("boot")
        ubman.ctrl('X')
        ubman.expect(['Booting a command list'])

    with ubman.log.section('Linux'):
        # Linux should start immediately
        ubman.expect(['Linux version'])

    with ubman.log.section('Ubuntu'):
        # Shortly later, we should see this banner
        ubman.expect(['Welcome to .*Ubuntu 24.04.1 LTS.*!'])

    ubman.restart_uboot()

@pytest.mark.boardspec('colibri-imx8x')
@pytest.mark.role('colibrimx8')
def test_distro_script(ubman):
    """Test that a selected board can boot into Llinux using a script"""
    with ubman.log.section('boot'):
        ubman.run_command('boot', wait_for_prompt=False)

    # This is the start of userspace
    ubman.expect(['Welcome to TDX Wayland'])

    # Shortly later, we should see this banner
    ubman.expect(['Colibri-iMX8X_Reference-Multimedia-Image'])

    ubman.restart_uboot()

@pytest.mark.boardspec('efi-arm_app64')
@pytest.mark.role('efi-aarch64')
def test_distro_arm_app_extlinux(ubman):
    """Test that the ARM EFI app can boot into Ubuntu 25.04 via extlinux"""
    with ubman.log.section('boot'):
        ubman.run_command('bootmeth order extlinux')
        ubman.run_command('boot', wait_for_prompt=False)

        ubman.expect(["Booting bootflow 'efi_media.bootdev.part_2' with extlinux"])
        ubman.expect(['Exiting EFI'])
        ubman.expect(['Booting Linux on physical CPU'])

    with ubman.log.section('initrd'):
        ubman.expect(['Starting systemd-udevd'])
        ubman.expect(['Welcome to Ubuntu 25.04!'])

    ubman.restart_uboot()

@pytest.mark.boardspec('efi-arm_app64')
@pytest.mark.role('efi-aarch64')
def test_distro_arm_app_efi(ubman):
    """Test that the ARM EFI app can boot into Ubuntu 25.04 via EFI"""
    with ubman.log.section('boot'):
        ubman.run_command('bootmeth order efi')
        ubman.run_command('boot', wait_for_prompt=False)

        ubman.expect(["Booting bootflow 'efi_media.bootdev.part_1' with efi"])

        # Press Escape to force GRUB to appear, even if the silent menu was
        # enabled by a previous boot
        ubman.send('\x1b')

    # Wait until we see the editor appear
    with ubman.log.section('grub'):
        ubman.expect(['ESC to return previous'])
        # ubman.expect(['The highlighted entry will be executed automatically in 29s'])

        # Press 'e' to edit the command line
        ubman.log.info("Pressing 'e'")
        ubman.send('e')
        for _ in range(10):
            ubman.ctrl('N')
        expected = '\tlinux\t/boot/vmlinuz-6.14.0-27-generic '
        expected += 'root=UUID=e5665fb4-e1de-4335-86da-357ad5422319 ro  '
        for _ in expected:
            ubman.ctrl('F')

        to_erase = 'quiet splash'
        for _ in to_erase:
            ubman.ctrl('D')
        ubman.ctrl('X')
        ubman.expect(['Booting a command list'])

    with ubman.log.section('exit boot-services'):
        ubman.expect(['EFI stub: Exiting boot services...'])

        ubman.log.info("boot")
        ubman.expect(['Booting Linux on physical CPU'])

    with ubman.log.section('initrd'):
        ubman.expect(['Freeing initrd memory:'])
        ubman.expect(['Run /init as init process'])

    with ubman.temporary_timeout(200 * 1000):
        ubman.expect(['Ubuntu 25.04 qarm ttyAMA0'])

    ubman.restart_uboot()
