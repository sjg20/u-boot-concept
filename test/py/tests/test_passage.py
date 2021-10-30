# SPDX-License-Identifier: GPL-2.0
# Copyright 2021 Google LLC

import pytest

@pytest.mark.buildconfigspec('of_passage')
def test_passage(ubman):
    """Test that the standard passage on ARM from SPL to U-Boot works."""

    response = ubman.run_command('bdinfo')
    assert 'devicetree  = passage' in response
