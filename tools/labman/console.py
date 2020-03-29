# SPDX-License-Identifier: GPL-2.0+
# Copyright 2020 Google LLC
# Written by Simon Glass <sjg@chromium.org>

class Console:
    """Console connection to a Dut

    This handles the various ways that a console can be connected
    """
    def __init__(self):
        pass

    def load(self, yam):
        self._type = yam['connection-type']
        self._port = yam['port']
