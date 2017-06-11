#
# Copyright (c) 2017 Google, Inc
#
# SPDX-License-Identifier:      GPL-2.0+
#
# Test of worker / master features

import time
import unittest

import master
import worker


HOST = 'localhost'


class TestNet(unittest.TestCase):
    def setUp(self):
        self.wkr = worker.Worker()
        self.wkr.start()
        self.mast = master.Master()
        self.mast2 = master.Master()

    def tearDown(self):
        self.wkr.stop()

    def testConnect(self):
        self.mast.open(HOST)
        self.mast.close()

    def testConnectTwo(self):
        self.mast.open(HOST)
        self.mast2.open(HOST)
        self.mast.close()
        self.mast2.close()

    def testEcho(self):
        self.mast.open(HOST)
        resp = self.mast.cmd_ping()
        self.assertEqual(bytearray('pong\n'), resp)
        self.mast.close()
