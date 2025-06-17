# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2015 Stephen Warren
# Copyright (c) 2015-2016, NVIDIA CORPORATION. All rights reserved.

"""
Logic to interact with the sandbox port of U-Boot, running as a sub-process.
"""

import os
import select
import time
from console_base import ConsoleBase, Timeout
from spawn import Spawn
import cmdsock


#TIMEOUT_MS = 1 * 1000
TIMEOUT_MS = 500 * 1000

# READY = 0

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
        self.ready = False
        self.cmd_result = None

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
        # print(fname)
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
            if self.config.redir_dev:
                cmd += ['-R', self.config.redir_dev]
        print('cmd', cmd)
        spawn = Spawn(cmd, cwd=self.config.source_dir, decode_signal=True,
                      no_launch=self.config.no_launch)

        # Connect the cmdsock
        if self.cmdsock:
            self.cmdsock.connect_to_sandbox(self.poll,
                                            bool(self.config.gdbserver))

        return spawn

    def start_uboot(self):
        # print('start u-boot')
        if self.cmdsock:
            self.cmdsock.start()
            # print('self.poll', self.cmdsock.sock, self.poll)

    def wait_ready(self):
        if not self.cmdsock:
            super().wait_ready()
            return
        # print('special wait ready', self.ready, id(self))
        # raise ValueError(f'wait ready {self.ready}')
        tstart_s = time.time()
        while not self.ready:
            tnow_s = time.time()
            # print('now', tnow_s, self.ready)
            tdelta_ms = (tnow_s - tstart_s) * 1000
            self.process_incoming()
            # print('###ready', self.ready)
            if tdelta_ms > TIMEOUT_MS and not self.config.no_timeouts:
                raise Timeout()
            # raise ValueError(f'handle {self.ready} {READY}')
            events = self.poll.poll(TIMEOUT_MS)
            # print('events', events)
            if not events:
                raise Timeout()
            for fd, event_mask in events:
                if fd == self.p.fd:
                    c = self.p.receive(1024)
                    self.add_input(c)
                else:
                    self.xfer_data(fd, event_mask)
            # print('--- checking')
            # for msg in self.cmdsock.get_msgs():
            #     print('kind', msg.WhichOneof('kind'))
            #     if msg.WhichOneof('kind') == 'start_resp':
            #         break
        # print('xxgot start_resp')
        # raise ValueError('ready')
        print('ready')
        self.buf = ''

    def xfer_data(self, fd, event_mask):
        if fd == self.cmdsock.sock.fileno():
            self.cmdsock.xfer_data(event_mask)

    def process_incoming(self, find_kind=None):
        if not self.cmdsock:
            return
        for msg in self.cmdsock.get_msgs():
            # print('\ngot', msg.WhichOneof('kind'))
            kind = msg.WhichOneof('kind')
            if kind == find_kind:
                return msg
            if kind == 'puts':
                print(f"1returning '{msg.puts.str}'")
                self.add_input(msg.puts.str)
            elif kind == 'start_resp':
                self.ready = True
                # global READY
                # READY = True
                # print('\n**& ready', self.ready, id(self))
                # raise ValueError('start_resp' + self.buf)
            elif kind == 'run_cmd_resp':
                self.cmd_result = msg.run_cmd_resp.result
            else:
                raise ValueError(f"Unknown kind '{kind}'")
        return None

    def wait_for_kind(self, find_kind):
        """Wait for a particular reply"""
        while True:
            self.xfer(TIMEOUT_MS)
            msg = self.process_incoming(find_kind)

    '''
    def poll_for_output(self, fd, event_mask):
        """Poll file descriptor for console output

        This can be overriden by subclasses, e.g. console_sandbox

        Args:
            fd (int): File descriptor to check
            event_mask (int): Event(s) which occured

        Return:
            str: Output (which may be an empty string if there is none)
        """
        # print('poll', fd, self.cmdsock.sock.fileno(), event_mask)
        if fd != self.cmdsock.sock.fileno():
            return ValueError(
                f'Internal error: fd {fd} fileno {self.cmdsock.sock.fileno()}')

        self.cmdsock.xfer(event_mask)
        text = ''
        for msg in self.cmdsock.get_msgs():
            # print('\ngot', msg.WhichOneof('kind'))
            kind = msg.WhichOneof('kind')
            if kind == 'puts':
                # print('returning', msg.puts.str)
                self.add_input(msg.puts.str)
            elif kind == 'start_resp':
                self.ready = True
        return text
    '''

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

    def run_command(self, cmd, wait_for_echo=True, send_nl=True,
                    wait_for_prompt=True, wait_for_reboot=False):
        if not self.cmdsock:
            return super().run_command(cmd, wait_for_echo, send_nl,
                                       wait_for_prompt, wait_for_reboot)

        # print('running')
        self.buf = ''
        self.cmdsock.run_command(cmd)
        self.wait_for_kind('run_cmd_resp')

        # Only strip \r\n; space/TAB might be significant if testing
        # indentation.
        return self.buf.strip('\r\n')

    def drain_console(self):
        if not self.cmdsock:
            super().drain_console()
            return
        self.xfer(0)
