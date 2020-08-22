# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2016 Google, Inc
# Written by Simon Glass <sjg@chromium.org>
#
# Entry-type module for Intel Management Engine binary blob
#

from binman.etype.blob_named_by_arg import Entry_blob_named_by_arg

class Entry_atf_bl31(Entry_blob_named_by_arg):
    """Entry containing an ARM Trusted Firmware (ATF) BL31 blob

    Properties / Entry arguments:
        - atf-bl31-path: Filename of file to read into entry

    This entry holds the run-time firmware started by ATF.
    """
    def __init__(self, section, etype, node):
        super().__init__(section, etype, node, 'atf-bl31')
        self.external = True
