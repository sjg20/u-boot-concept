# SPDX-License-Identifier: GPL-2.0+
# Copyright 2020 Google LLC
# Written by Simon Glass <sjg@chromium.org>

# Tests for Console class

import unittest

from labman. console import Console

PORT = '/dev/ttyusb_port0'

class ConsoleTest(unittest.TestCase):
    def testLoad(self):
        yam = {'connection-type': 'usb-uart', 'port': PORT}
        con = Console(self)
        con.load(yam)
        self.assertEqual(Console.CTYPE_USB_UART, con._type)
        self.assertEqual(PORT, con._port)
        self.assertEqual('console %s/%s' % (Console.CTYPE_USB_UART, PORT),
                         str(con))
        self.assertEqual(PORT, con.get_uart())

    def Raise(self, msg):
        raise ValueError('test: %s' % msg)

    def testNoConnectionType(self):
        yam = {'connection-type': 'blather', 'port': PORT}
        con = Console(self)
        with self.assertRaises(ValueError) as e:
            con.load(yam)
        self.assertIn("test: console None/None: Invalid connection-type",
                      str(e.exception))

    def testBadConnectionType(self):
        yam = {'port': PORT}
        con = Console(self)
        with self.assertRaises(ValueError) as e:
            con.load(yam)
        self.assertIn("test: console 0/None: Missing connection-type",
                      str(e.exception))

    def testNoPort(self):
        yam = {'connection-type': 'usb-uart'}
        con = Console(self)
        with self.assertRaises(ValueError) as e:
            con.load(yam)
        self.assertIn("test: console %d/None: Missing port" %
                      Console.CTYPE_USB_UART, str(e.exception))
