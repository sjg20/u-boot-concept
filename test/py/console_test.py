#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
# Copyright 2025 Simon Glass <sjg@chromium.org>

"""Tests for the console_sandbox and console_board classes"""

import os
import tempfile
import unittest

import console_sandbox
import multiplexed_log

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
        self.build_dir = '/tmp/b/sandbox'
        self.dtb = '/tmp/b/sandbox/arch/sandbox/dts/test.dtb'
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

    def xtest_sandbox(self):

        # Create a fixture for to use, a basic version of ubconfig
        ubc = Ubconfig(self.tmpdir)

        cons = console_sandbox.ConsoleSandbox(ubc.log, ubc)

        cons.ensure_spawned()

        val = cons.run_command('echo fred')
        self.assertEqual('fred', val)

        ubc.log.close()
        cons.cleanup_spawn()

    def test_sandbox_cmdsock(self):
        # Create a fixture for to use, a basic version of ubconfig
        ubc = Ubconfig(self.tmpdir, cmdsock=True)
        ubc.no_timeouts = True
        ubc.redir_dev = '/tmp/ttyV0'

        cons = console_sandbox.ConsoleSandbox(ubc.log, ubc)

        cons.ensure_spawned()

        val = cons.run_command('echo fred')
        self.assertEqual('fred', val)

        ubc.log.close()
        cons.cleanup_spawn()

if __name__ == "__main__":
    unittest.main()
