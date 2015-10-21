#!/usr/bin/env python

# Copyright (c) 2015 Stephen Warren
# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# SPDX-License-Identifier: GPL-2.0

import os
import os.path
import sys

sys.argv.pop(0)

args = ["py.test", os.path.dirname(__file__)]
args.extend(sys.argv)

try:
    os.execvp("py.test", args)
except:
    import traceback
    traceback.print_exc()
    print >>sys.stderr, """
exec(py.test) failed; perhaps you are missing some dependencies?
See test/md/README.md for the list."""
