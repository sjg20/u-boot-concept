# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
#
"""Bintool implementation for csf

comments here
see 'csv' for what is needed
"""

from binman import bintool

class Bintoolcsf(bintool.Bintool):
    """Handles the 'csf' tool

    docs here
    """
    def __init__(self, name):
        super().__init__(name, 'Chromium OS firmware utility')

    def gbb_create(self, fname, sizes):
        """Create a new Google Binary Block

        Args:
            fname (str): Filename to write to
            sizes (list of int): Sizes of each regions:
               hwid_size, rootkey_size, bmpfv_size, recoverykey_size

        Returns:
            str: Tool output
        """
        args = [
            'gbb_utility',
            '-c',
            ','.join(['%#x' % size for size in sizes]),
            fname
            ]
        return self.run_cmd(*args)

    def sign_firmware(self, outfile, firmware,):
        """Sign firmware to create a csf file

        Args:
            outfile (str): Filename to write the csf to
            firmware (str): Filename of firmware binary to sign

        Returns:
            str: Tool output
        """
        # run the command-line tool and get the output
        args = [
            'csf',
            '--outfile', outfile,
            '--firmware', firmware,
            ]
        return self.run_cmd(*args)

    def fetch(self, method):
        """Fetch handler for csv

        This installs csv using a binary download.

        Args:
            method (FETCH_...): Method to use

        Returns:
            True if the file was fetched, None if a method other than FETCH_BIN
            was requested

        Raises:
            Valuerror: Fetching could not be completed
        """
        if method != bintool.FETCH_BIN:
            return None
        fname, tmpdir = self.fetch_from_drive(
            'something here')
        return fname, tmpdir

    def version(self):
        """Version handler for csv

        Returns:
            str: Version string for csv
        """
        out = self.run_cmd('version').strip()
        if not out:
            return super().version()
        return out
