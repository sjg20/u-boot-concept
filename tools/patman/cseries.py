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

import pygit2

from patman import series
from patman.series import Series
from u_boot_pylib import gitutil
from u_boot_pylib import tout


class Cseries:
    """Database with information about series

    This class handles database read/write as well as operations in a git
    directory to update series information.
    """
    def __init__(self, topdir=None):
        self.topdir = topdir
        self.gitdir = os.path.join(topdir, '.git')
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
            self.cur.execute(
                'CREATE TABLE series (id INTEGER PRIMARY KEY AUTOINCREMENT,'
                'name, desc, archived BIT)')

            # version (int): Version number of the link
            self.cur.execute(
                'CREATE TABLE patchwork (id INTEGER PRIMARY KEY AUTOINCREMENT,'
                'version INTEGER, link, series_id INTEGER,'
                'FOREIGN KEY (series_id) REFERENCES series (id))')
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

    def get_series_dict(self, include_archived=False):
        """Get a dict of Series objects from the database

        Return:
            OrderedDict:
                key: series name
                value: Series
        """
        res = self.cur.execute('SELECT name, desc FROM series ' +
            ('WHERE archived = 0' if not include_archived else ''))
        sdict = OrderedDict()
        for name, desc in res.fetchall():
            ser = Series()
            ser.name = name
            ser.desc = desc
            sdict[name] = ser
        return sdict

    def add_series(self, ser):
        """Add a series to the database

        Args:
            ser (Series): Series to add
        """
        # First check we have a branch with this name
        if not gitutil.check_branch(ser.name, git_dir=self.gitdir):
            raise ValueError(f"No branch named '{ser.name}'")
        res = self.cur.execute(
            'INSERT INTO series (name, desc, archived) '
            f"VALUES ('{ser.name}', '{ser.desc}', 0)")
        self.con.commit()
        ser.id = self.cur.lastrowid

    def get_series_by_name(self, name):
        """Get a Series object from the database by name

        Args:
            name (str): Name of series to get

        Return:
            Series: Object containing series info, or None if none
        """
        res = self.cur.execute(
            f"SELECT id, name, desc FROM series WHERE name = '{name}'")
        all = res.fetchall()
        if not all:
            return None
        if len(all) > 1:
            raise ValueError('Expected one match, but multiple matches found')
        ser = Series()
        ser.id, ser.name, ser.desc = desc = all[0]
        return ser

    def add_link(self, ser, version, link, update_commit):
        """Add / update a series-link link for a series

        Args:
            ser (Series): Series object containing the ID
            version (int): Version number
            link (str): Patchwork link-string for the series
            update_commit (bool): True to update the current commit with the
                link
        """
        if update_commit:
            if gitutil.get_branch(self.gitdir) != ser.name:
                raise ValueError(f"Cannot update as not on branch '{ser.name}'")
            repo = pygit2.init_repository(self.gitdir)
            commit = repo.head.peel(pygit2.GIT_OBJ_COMMIT)
            new_msg = commit.message + f'\nSeries-links: {link}'
            amended = repo.amend_commit(commit, 'HEAD', message=new_msg)
            repo.head.set_target(amended)

        res = self.cur.execute(
            'INSERT INTO patchwork (version, link, series_id) VALUES'
            f"('{version}', '{link}', {ser.id})")
        self.con.commit()

    def get_link(self, ser, version):
        res = self.cur.execute('SELECT link FROM patchwork WHERE '
            f"series_id = {ser.id} AND version = '{version}'")
        all = res.fetchall()
        if not all:
            return None
        if len(all) > 1:
            raise ValueError('Expected one match, but multiple matches found')
        return all[0][0]

    def parse_series(self, name):
        """Parse the name of a series, or detect it from the current branch

        Args:
            name (str or None): name of series

        Return:
            Series: New object with the name and id set
        """
        if not name:
            name = gitutil.get_branch(self.gitdir)
        ser = self.get_series_by_name(name)
        if not ser:
            ser = Series()
            ser.name = name
        return ser

    def set_archived(self, series, archived):
        """Set whether a series is archived or not

        Args:
            series (str): Name of series to use, or None to use current branch
            archived (bool): Whether to mark the series as archived or
                unarchived
        """
        ser = self.parse_series(series)
        if not ser.id:
            raise ValueError(f"Series '{ser.name}' not found in database")
        ser.archived = archived
        res = self.cur.execute(
            f'UPDATE series SET archived = {int(archived)} WHERE '
            f'id = {ser.id}')
        self.con.commit()
