# SPDX-License-Identifier: GPL-2.0+
# Copyright 2020 Google LLC

from tools.labman import console

class Dut:
    """Individual board in the lab, upon which we want to run tests.

    DUT stands for device-under-test. It means the device that is being tested.

    If a board can be used for different builds, it should still only appear
    once in the lab.

    DUTs have a particular hardware configuration (e.g processor, memory) and
    have connections with in the lab (e.g. for serial UART).

    Properties:
        _name: Name of the DUT (e.g. 'rpi_3')
        _desc: Description of the DUT (e.g. 'Raspberry Pi 3b')
        _cons: Console to talk to the DUT
    """
    def __init__(self, name):
        self._name = name
        self._desc = None
        self._cons = None

    def load(self, yam):
        cons_yam = yam.get('console')
        if cons_yam:
            self._cons = console.Console()
            self._cons.load(cons_yam)

    def show(self):
        print('%10  %s' % (self.name, self.desc))
