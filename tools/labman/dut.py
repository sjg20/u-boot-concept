# SPDX-License-Identifier: GPL-2.0+
# Copyright 2020 Google LLC

class Dut:
    """
    A DUT (device-under-test) is an individual board in the lab, upon which we
    want to run tests.

    DUTs have a particular hardware configuration (e.g processor, memory) and
    have connections with in the lab (e.g. for serial UART).
    """
    def __init__(self, name):
        self._name = name
