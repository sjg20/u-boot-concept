# SPDX-License-Identifier: GPL-2.0+
# Copyright 2019 Google LLC
# Written by Simon Glass <sjg@chromium.org>
#
# Entry-type module for Intel Firmware Support Package binary blob (T section)
#

from entry import Entry
from blob import Entry_blob

class Entry_intel_fsp_f(Entry_blob):
    """Entry containing Intel Firmware Support Package (FSP) temp-raminit

    Properties / Entry arguments:
        - filename: Filename of file to read into entry

    This file contains a binary blob which is used on some devices to set up
    an initial on-chip SRAM area which can be used for a stack and other
    purposes. This is the first stage of getting the device to start. U-Boot
    executes this code very early in boot since it is not possible to set up
    the SRAM using U-Boot open-source code. Documentation is typically not
    available in sufficient detail to allow this.

    An example filename is 'fsp_t.bin'

    See README.x86 for information about x86 binary blobs.
    """
    def __init__(self, section, etype, node):
        Entry_blob.__init__(self, section, etype, node)
