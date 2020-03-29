# SPDX-License-Identifier: GPL-2.0+
# Copyright 2020 Google LLC
# Written by Simon Glass <sjg@chromium.org>

import yaml

class Lab:
    """A colection of DUTs, builders and the infrastructure to connect them"""
    def __init__(self):
        self._name = None

    def read(self, fname):
        with open(fname) as inf:
            data = inf.read()
        yam = yaml.load(data, Loader=yaml.SafeLoader)
        load_duts(yam)
        print('yam', yam)
