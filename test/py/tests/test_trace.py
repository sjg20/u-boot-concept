# SPDX-License-Identifier: GPL-2.0
# Copyright 2022 Google LLC
# Written by Simon Glass <sjg@chromium.org>

import os
import pytest

import u_boot_utils as util

# This is needed for Azure, since the default '..' directory is not writeable
TMPDIR = '/tmp/test_trace'

@pytest.mark.slow
@pytest.mark.boardspec('sandbox')
def test_trace(u_boot_console):
    """Test we can build sandbox with trace, collect and process a trace"""
    cons = u_boot_console

    env = dict(os.environb)
    env['FTRACE'] = '1'
    out = util.run_and_log(
        cons, ['./tools/buildman/buildman', '-m', '--board', 'sandbox',
               '-o', TMPDIR], ignore_errors=True, env=env)
