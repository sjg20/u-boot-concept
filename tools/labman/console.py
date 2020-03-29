# SPDX-License-Identifier: GPL-2.0+
# Copyright 2020 Google LLC
# Written by Simon Glass <sjg@chromium.org>

class Console:
    """Console connection to a Dut

    This handles the various ways that a console can be connected

    Properties:
        _parent: Parent object (e.g. a Dut)
        _type: Type of console (CONS_TYPES)
        _port: Device to use as a console (e.g. '/dev/ttyusb_port1')
    """
    CONS_TYPES = {
        'none': 0,
        'usb-uart': 1,
        }

    def __init__(self, parent):
        self._parent_= parent

    def Raise(self, msg):


    def load(self, yam):
        cname = yam.get('connection-type')
        if not cname:
            Raise('Missing connection-type')
        self._type = self.CONS_TYPES.get(cname)
        if self._type is None:
            self.Raise("Invalid connection-type '%s'" % cname)
        self._port = yam['port']
