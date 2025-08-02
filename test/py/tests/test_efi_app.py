# SPDX-License-Identifier:	GPL-2.0+
# Copyright 2025 Simon Glass <sjg@chromium.org>

"""Tests for the EFI app"""

import pytest

@pytest.mark.buildconfigspec('efi_app_64bit')
def test_efi_app(ubman):
    """Test that the EFI app starts up correctly"""
