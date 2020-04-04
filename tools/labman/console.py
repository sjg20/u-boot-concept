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
    CTYPE_NONE, CTYPE_USB_UART = range(2)

    CONS_TYPES = {
        'none': CTYPE_NONE,
        'usb-uart': CTYPE_USB_UART,
        }

    def __init__(self, parent):
        self._parent = parent
        self._type = 0
        self._port = None

    def __str__(self):
        return 'console %s/%s' % (self._type, self._port)

    def Raise(self, msg):
        self._parent.Raise('%s: %s' % (str(self), msg))

    def load(self, yam):
        cname = yam.get('type')
        if not cname:
            self.Raise('Missing type')
        self._type = self.CONS_TYPES.get(cname)
        if self._type is None:
            self.Raise("Invalid type '%s'" % cname)
        self._port = yam.get('port')
        if self._port is None:
            self.Raise("Missing port '%s'" % cname)

    def get_uart(self):
        return self._port
