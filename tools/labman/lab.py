# SPDX-License-Identifier: GPL-2.0+
# Copyright 2020 Google LLC
# Written by Simon Glass <sjg@chromium.org>

import yaml

from tools.labman import dut

class Lab:
    """A colection of DUTs, builders and the infrastructure to connect them

    Properties:
        _name: Name of the lab. This should be short, ideally a nickname.
        _duts: List of available DUTs
    """
    def __init__(self):
        self._name = None
        self._desc = None
        self._duts = {}

    def read(self, fname):
        with open(fname) as inf:
            data = inf.read()
        yam = yaml.load(data, Loader=yaml.SafeLoader)
        self.load_duts(yam['duts'])
        #print('yam', yam)

    def load_duts(self, yam):
        for name, dut_yam in yam.items():
            dutt = dut.Dut(name)
            dutt.load(dut_yam)
            self._duts[name] = dutt

    def show_list(self):
        print('DUTs: ')
        for dutt in sorted(self._duts):
            dutt.show()
