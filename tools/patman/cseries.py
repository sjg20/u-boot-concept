# SPDX-License-Identifier: GPL-2.0+
#
# Copyright 2025 Google LLC
#
"""Handles the 'series' subcommand
"""

from collections import OrderedDict
import os
import sqlite3
from sqlite3 import OperationalError

from patman import series
from patman.series import Series
from u_boot_pylib import gitutil
from u_boot_pylib import tout


class Cseries:
    def __init__(self, topdir=None):
        self.topdir = topdir
        self.con = None
        self.cur = None

    def check_database(self, con):
        """Check that the database has the required tables and is up-to-date

        Args:
            con (sqlite3.Connection): Database connection
        """
        # Check if a series table is present
        self.cur = self.con.cursor()
        try:
            res = self.cur.execute('SELECT name FROM series')
        except OperationalError:
            self.cur.execute('CREATE TABLE series(name, desc)')
        return self.cur

    def open_database(self):
        if not self.topdir:
            self.topdir = gitutil.get_top_level()
            if not self.topdir:
                raise ValueError('No git repo detected in current directory')
        fname = f'{self.topdir}/.patman.db'
        if not os.path.exists(fname):
            tout.warning(f'Creating new database {fname}')
        self.con = sqlite3.connect(fname, autocommit=False)

        self.cur = self.check_database(self.con)
        return self.cur

    def close_database(self):
        self.con.close()

    def get_series_dict(self):
        """Get a dict of Series objects from the database

        Return:
            OrderedDict:
                key: series name
                value: Series
        """
        res = self.cur.execute('SELECT name, desc FROM series')
        sdict = OrderedDict()
        for name, desc in res.fetchall():
            ser = Series()
            ser.name = name
            ser.desc = desc
            sdict[name] = ser
        return sdict

    def add_series(self, ser):
        """Add a series to the database

        ser (Series): Series to add
        """
        # First check we have a branch with this name

        res = self.cur.execute(
            f"INSERT INTO series VALUES ('{ser.name}', '{ser.desc}')")
        self.con.commit()

    def do_add(self, name, desc):
        sdict = self.get_series_dict()

        if name in sdict:
            raise ValueError("Series 'f{name}' already exists'")
        ser = Series()
        ser.name = name
        ser.desc = desc
        self.add_series(ser)
