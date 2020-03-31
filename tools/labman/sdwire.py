# SPDX-License-Identifier: GPL-2.0+
# Copyright 2020 Google LLC
# Written by Simon Glass <sjg@chromium.org>

import re
import time

from patman import command

class Sdwire:
    """An Sdwire board which can switch a uSD card between DUT and test system

    Properties:
        _name: Name of the sdwire. This should be short, ideally a nickname.
        _serial: Serial number of the Sdwire
    """
    ORIG_IDS = ['--vendor=0x0403', '--product=0x6015']

    def __init__(self, name):
        self._name = name

    def __str__(self):
        return 'sdwire %s' % self._name

    def Raise(self, msg):
        raise ValueError('%s: %s' % (str(self), msg))

    def sdmux_ctrl(self, *args):
        while True:
            try:
                out = command.Output('sd-mux-ctrl', *args)
                return out
            except Exception as e:
                print('Error: %s' % e)
            time.sleep(1)

    def parse_serial(self, line):
        m = re.search(r'Serial: ([A-Z0-9a-z]+),', line)
        if not m:
            self.Raise("Unable to find serial number in '%s'" % line)
        old_serial = m.group(1)
        return old_serial

    def list_matching(self, ids):
        out = self.sdmux_ctrl(*ids, '--list')
        lines = out.splitlines()
        m = re.search('Number of FTDI devices found: ([0-9]+)', lines[0])
        if not m:
            self.Raise("Expected device count in first line" % lines)
        found = int(m.group(1))
        serial = None
        if found == 1:
            serial = self.parse_serial(lines[1])
        return found, serial

    def get_serial(self):
        found, serial = self.list_matching(self.ORIG_IDS)
        if found != 1:
            self.Raise("Expected to find one SDwire, found %s" % found)
        return serial

    def provision(self, new_serial):
        """Provision an SDwire ready for use

        This only works if there is a single SDwire connected.
        """
        # Get the current serial number
        old_serial = self.get_serial()

        # Program in the new serial number
        out = self.sdmux_ctrl('--device-serial=%s' % old_serial, *self.ORIG_IDS,
                              '--device-type=sd-mux',
                              '--set-serial=%s' % new_serial)

        # Wait for the device to re-appear with the new details
        print('Unplug the SDwire...', end='', flush=True)
        while True:
            found, serial = self.list_matching(self.ORIG_IDS)
            if not found:
                break
            time.sleep(1)

        print('\nInsert the SDwire...', end='', flush=True)
        while True:
            found, serial = self.list_matching([])
            if found == 1:
                break
            time.sleep(1)
        if serial != new_serial:
            self.Raise("Expected serial '%s' but got '%s'" %
                       (new_serial, serial))
        print('\nProvision complete')
