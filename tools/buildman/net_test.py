#
# Copyright (c) 2017 Google, Inc
#
# SPDX-License-Identifier:      GPL-2.0+
#
# Test of worker / master features

import time
import unittest

import master
import net_cmd
import worker


HOST = 'localhost'
BOARDS = ['snow', 'rpi']
COMMITS = ['3b74cc4', '283b738']


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
        resp = net_cmd.CmdPing(self.mast).req()
        self.assertEqual(bytearray('pong\n'), resp)
        self.mast.close()

    def testDeadWorker(self):
        """Check we can timeout when the worker does not respond"""
        with worker.play_dead(self.wkr):
            self.mast.open(HOST)
            resp = net_cmd.CmdPing(self.mast).req()
            self.assertEqual(None, resp)
            self.mast.close()

    def testSetBoards(self):
        self.mast.open(HOST)
        resp = net_cmd.CmdSetBoards(self.mast, BOARDS).req()
        self.assertEqual('ok\n', str(resp))
        self.mast.close()
        self.assertEqual(BOARDS, self.wkr.boards)

    def testSetCommits(self):
        self.mast.open(HOST)
        resp = net_cmd.CmdSetCommits(self.mast, COMMITS).req()
        self.assertEqual('ok\n', str(resp))
        self.mast.close()
        self.assertEqual(COMMITS, self.wkr.commits)
