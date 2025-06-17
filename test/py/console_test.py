#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
# Copyright 2025 Simon Glass <sjg@chromium.org>

"""Tests for the console_sandbox and console_board classes"""

import os
import tempfile
import unittest

import console_sandbox
import multiplexed_log

BUILD_DIR = '/tmp/b/sandbox'
DTB_FILE = f'{BUILD_DIR}/arch/sandbox/dts/test.dtb'


class Ubconfig():
    """Create a test version of the ubconfig object in test.py"""

    def __init__(self, tmpdir, cmdsock=None):
        # Set up a dummy build config
        self.tmpdir = tmpdir
        self.buildconfig = {
            'config_sys_prompt': '"=>"',
            }
        self.gdbserver = ''
        # self.gdbserver = 'localhost:1234'
        #self.build_dir = os.path.join(self.tmpdir, 'build')
        self.build_dir = BUILD_DIR
        self.dtb = DTB_FILE
        self.cmdsock = cmdsock
        self.source_dir = os.path.join(self.tmpdir, 'source')
        if not os.path.exists(self.source_dir):
            os.mkdir(self.source_dir)
        self.use_running_system = False
        self.env = {}
        self.result_dir = os.path.join(self.tmpdir, 'results')
        if not os.path.exists(self.result_dir):
            os.mkdir(self.result_dir)
        self.log = multiplexed_log.Logfile(self.result_dir + '/test-log.html')
        self.no_timeouts = False
        self.no_launch = False
        self.redir_dev = None


class TestConsole(unittest.TestCase):
    """Test for the console_base class and subclasses"""
    def setUp(self):
        pass

    @classmethod
    def setUpClass(cls):
        """Set up things used by most/all tests"""
        # cls.tmpdir = tempfile.mkdtemp(prefix='ctest.')
        cls.tmpdir = '/tmp/test'

    def test_sandbox(self):
        """Test sandbox running via a pty"""
        # Create a fixture to use, a basic version of ubconfig
        ubc = Ubconfig(self.tmpdir)

        cons = console_sandbox.ConsoleSandbox(ubc.log, ubc)

        cons.ensure_spawned()

        val = cons.run_command('echo fred')
        self.assertEqual('fred', val)

        ubc.log.close()
        cons.cleanup_spawn()

    def test_sandbox_cmdsock(self):
        # Create a fixture to use, a basic version of ubconfig
        ubc = Ubconfig(self.tmpdir, cmdsock=True)
        ubc.no_timeouts = True
        ubc.redir_dev = '/tmp/ttyV0'

        cons = console_sandbox.ConsoleSandbox(ubc.log, ubc)
        cons.ensure_spawned()

        val = cons.run_command('echo fred')
        self.assertEqual('fred', val)

        ubc.log.close()
        cons.cleanup_spawn()

    def test_sandbox_cmdsock_nolaunch(self):
        # Create a fixture to use, a basic version of ubconfig
        ubc = Ubconfig(self.tmpdir, cmdsock=True)
        ubc.no_timeouts = True
        ubc.redir_dev = '/tmp/ttyV0'
        # ubc.no_launch = True
        # ubc.no_timeouts = True

        cons = console_sandbox.ConsoleSandbox(ubc.log, ubc)

        args = [f'{BUILD_DIR}/u-boot', '-v', '-d', DTB_FILE,
                '--cmdsock', f'{ubc.result_dir}/cmd.sock', '-R', '/tmp/ttyV0']
        print('args', args)
        pid = os.fork()
        if pid == 0:
            try:
                os.execvp(args[0], args)
            except Exception as exc:
                raise ValueError(f'Child exception: {str(exc)}') from exc
            os._exit(0)

        cons.ensure_spawned()

        val = cons.run_command('echo fred')
        self.assertEqual('fred', val)

        ubc.log.close()
        cons.cleanup_spawn()

    def test_sandbox_printenv(self):
        """Check that the printenv command can be used"""
        ubc = Ubconfig(self.tmpdir, cmdsock=True)
        ubc.no_timeouts = True
        ubc.redir_dev = '/tmp/ttyV0'
        # ubc.no_launch = True
        # ubc.no_timeouts = True

        cons = console_sandbox.ConsoleSandbox(ubc.log, ubc)
        cons.ensure_spawned()

        self.maxDiff = None
        resp = cons.run_command('printenv')
        self.assertEqual('''arch=sandbox
baudrate=115200
board=sandbox
board_name=sandbox
bootcmd=bootflow scan -lb
bootdelay=2
bootm_size=0x10000000
cpu=sandbox
env_fdt_path=/config/environment
eth2addr=02:00:11:22:33:48
eth3addr=02:00:11:22:33:45
eth4addr=02:00:11:22:33:48
eth5addr=02:00:11:22:33:46
eth6addr=02:00:11:22:33:47
eth7addr=02:00:11:22:33:48
eth8addr=02:00:11:22:33:49
ethaddr=02:00:11:22:33:44
fdt_addr_r=0xc00000
fdtcontroladdr=8b1b310
from_fdt=yes
ipaddr=192.0.2.1
ipaddr2=192.0.2.3
ipaddr3=192.0.2.4
ipaddr5=192.0.2.6
ipaddr6=192.0.2.7
ipaddr7=192.0.2.8
kernel_addr_r=0x1000000
loadaddr=0x0
pxefile_addr_r=0x2000
ramdisk_addr_r=0x2000000
scriptaddr=0x1000
stderr=serial,vidconsole
stdin=serial,cros-ec-keyb,usbkbd
stdout=serial,vidconsole
usb_ignorelist=0x1050:*,

Environment size: 756/8188 bytes''', '\n'.join(resp.splitlines()))
        resp = cons.run_command('printenv board_name')
        self.assertEqual('board_name=sandbox', resp)

        ubc.log.close()
        cons.cleanup_spawn()


if __name__ == "__main__":
    unittest.main()
