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
    def __init__(self):
        # Set up a dummy build config
        self.buildconfig = {
            'config_sys_prompt': '=>',
            }


class TestConsole(unittest.TestCase):
    """Test for the console_base class and subclasses"""
    def setUp(self):
        pass

    @classmethod
    def setUpClass(cls):
        """Set up things used by most/all tests"""
        cls.tmpdir= tempfile.mkdtemp(prefix='ctest.')

    def test_andbox(self):
        result_dir = os.path.join(self.tmpdir, 'results')
        os.mkdir(result_dir)
        log = multiplexed_log.Logfile(result_dir + '/test-log.html')

        # Create a fixture for to use, a basic version of ubconfig
        ubc = Ubconfig()

        cons = console_sandbox.ConsoleSandbox(log, ubc)
        log.close()
        cons.cleanup_spawn()

if __name__ == "__main__":
    unittest.main()
