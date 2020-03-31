# SPDX-License-Identifier: GPL-2.0+
# Copyright 2020 Google LLC
# Written by Simon Glass <sjg@chromium.org>

from tools.patman import command

class Sdwire:
    """An Sdwire board which can switch a uSD card between DUT and test system

    Properties:
        _name: Name of the sdwire. This should be short, ideally a nickname.
        _serial: Serial number of the Sdwire
    """
    def __init__(self, name):
        self._name = name

    def __str__(self):
        return 'sdwire %s' % self._name

    def Raise(self, msg):
        raise ValueError('%s: %s' % (str(self), msg))

    def provision(self, serial):
        """Provision an SDwire ready for use

        This only works if there is a single SDwire connected.
        """
        out = command.Output('sd-mux-ctrl', '--list', '--vendor=0x0403',
                             '--product=0x6015')
        lines = out.splitlines()
        if lines != 2:
            self.Raise("Unexpected sd-mux-ctrl output '%s'" % out)
#sudo sd-mux-ctrl --device-serial=AL018T40 --vendor=0x0403 --product=0x6001 --device-type=sd-mux --set-serial=odroid_u3_1
