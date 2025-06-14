# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2015 Stephen Warren
# Copyright (c) 2015-2016, NVIDIA CORPORATION. All rights reserved.

"""
Logic to interact with the sandbox port of U-Boot, running as a sub-process.
"""

import os
import select
import time
from console_base import ConsoleBase
from spawn import Spawn
import cmdsock


class ConsoleSandbox(ConsoleBase):
    """Represents a connection to a sandbox U-Boot console, executed as a sub-
    process."""

    def __init__(self, log, config):
        """Initialize a U-Boot console connection.

        Args:
            log (multiplexed_log.Logfile): Log file to write to
            config (ArbitraryAttributeContainer): ubconfig "configuration"
                object as defined in conftest.py
        """
        super().__init__(log, config, max_fifo_fill=1024)
        self.sandbox_flags = []
        self.use_dtb = True
        self.cmdsock = None

    def get_spawn(self):
        """Connect to a fresh U-Boot instance.

        A new sandbox process is created, so that U-Boot begins running from
        scratch.

        Returns:
            A spawn.Spawn object that is attached to U-Boot.
        """
        bcfg = self.config.buildconfig
        config_spl = bcfg.get('config_spl', 'n') == 'y'
        config_vpl = bcfg.get('config_vpl', 'n') == 'y'
        if config_vpl:
            # Run TPL first, which runs VPL
            fname = '/tpl/u-boot-tpl'
        else:
            fname = '/spl/u-boot-spl' if config_spl else '/u-boot'
        print(fname)
        cmd = []
        if self.config.gdbserver:
            cmd += ['gdbserver', self.config.gdbserver]
        cmd += [self.config.build_dir + fname, '-v']
        if self.use_dtb:
            cmd += ['-d', self.config.dtb]
        cmd += self.sandbox_flags
        cmdsock_fname = None
        if self.config.cmdsock:
            cmdsock_fname = os.path.join(self.config.result_dir, 'cmd.sock')
            cmd += ['--cmdsock', cmdsock_fname]
            self.cmdsock = cmdsock.Cmdsock(cmdsock_fname)
        spawn = Spawn(cmd, cwd=self.config.source_dir, decode_signal=True)

        # Connect the cmdsock
        if self.cmdsock:
            self.cmdsock.connect_to_sandbox()
            self.poll.register(self.cmdsock.sock, select.POLLIN |
                               select.POLLOUT | select.POLLPRI |
                               select.POLLERR | select.POLLHUP |
                               select.POLLNVAL)

        return spawn

    def start_uboot(self):
        print('start u-boot')
        if self.cmdsock:
            self.cmdsock.start()
            print('self.poll', self.cmdsock.sock, self.poll)

    def poll_for_output(self, fd, event_mask):
        """Poll file descriptor for console output

        This can be overriden by subclasses, e.g. console_sandbox

        Args:
            fd (int): File descriptor to check
            event_mask (select.poll bitmask): Event(s) which occured

        Return:
            str: Output (which may be an empty string if there is none)
        """
        if fd != self.cmdsock.sock:
            return ''

        msg = self.cmdsock.xfer(event_mask)
        if msg:
            print('got', msg, msg.WhichOneof('kind'))
            if msg.WhichOneof('kind') == 'puts':
                print('returning', msg.puts.str)
                return msg.puts.str
        return ''


    def restart_uboot_with_flags(self, flags, expect_reset=False, use_dtb=True):
        """Run U-Boot with the given command-line flags

        Args:
            flags (list of str): List of flags to pass
            expect_reset (bool): Indication whether this boot is expected
                to be reset while the 1st boot process after main boot before
                prompt. False by default.
            use_dtb (bool): True to use a device tree file, False to run without
                one

        Returns:
            A spawn.Spawn object that is attached to U-Boot.
        """
        try:
            self.sandbox_flags = flags
            self.use_dtb = use_dtb
            return self.restart_uboot(expect_reset)
        finally:
            self.sandbox_flags = []
            self.use_dtb = True

    def kill(self, sig):
        """Send a specific Unix signal to the sandbox process.

        Args:
            sig (int): Unix signal to send to the process
        """
        self.poll.unregister(self.cmdsock.sock)
        self.log.action(f'kill {sig}')
        self.p.kill(sig)

    def validate_exited(self):
        """Determine whether the sandbox process has exited.

        If required, this function waits a reasonable time for the process to
        exit.

        Returns:
            Boolean indicating whether the process has exited.
        """
        p = self.p
        self.p = None
        for _ in range(100):
            ret = not p.isalive()
            if ret:
                break
            time.sleep(0.1)
        p.close()
        return ret
