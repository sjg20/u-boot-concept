# SPDX-License-Identifier: GPL-2.0+
#
# Copyright 2025 Simon Glass <sjg@chromium.org>
#
"""Handles the patman database
"""

import os
import sqlite3

from u_boot_pylib import tout

# Schema version
LATEST = 2


class Database:
    """Database of information used by patman"""
    def __init__(self, db_path):
        self.con = None
        self.cur = None
        self.db_path = db_path

    def start(self):
        """Open the database read for use"""
        self.open_it()
        self.ensure_exists()
        self.migrate_to(LATEST)

    def open_it(self):
        """Open the database read for use"""
        if not os.path.exists(self.db_path):
            tout.warning(f'Creating new database {self.db_path}')
        self.con = sqlite3.connect(self.db_path, autocommit=False)
        self.cur = self.con.cursor()

    def create_v0(self):
        tout.info('Create database v0')
        self.cur.execute(
            'CREATE TABLE series (id INTEGER PRIMARY KEY AUTOINCREMENT,'
            'name UNIQUE, desc, archived BIT)')

        # Provides a series_id/version pair, which is used to refer to a
        # particular series version sent to patchwork. This stores the link
        # to patchwork
        self.cur.execute(
            'CREATE TABLE ser_ver (id INTEGER PRIMARY KEY AUTOINCREMENT,'
            'series_id INTEGER, version INTEGER, link,'
            'FOREIGN KEY (series_id) REFERENCES series (id))')

        self.cur.execute(
            'CREATE TABLE upstream (name UNIQUE, url, is_default BIT)')

        # change_id is the Change-Id
        # patch_id is the ID of the patch on the patchwork server
        self.cur.execute(
            'CREATE TABLE pcommit (id INTEGER PRIMARY KEY AUTOINCREMENT,'
            'svid INTEGER, seq INTEGER, subject, patch_id INTEGER, '
            'change_id, state, num_comments INTEGER, '
            'FOREIGN KEY (svid) REFERENCES ser_ver (id))')

        self.cur.execute(
            'CREATE TABLE settings (name UNIQUE, proj_id INT, link_name)')
        self.con.commit()

    def _migrate_to_v1(self):
        """Add a schema_version table"""
        self.cur.execute('CREATE TABLE schema_version (version INTEGER)')

    def _migrate_to_v2(self):
        """Store the number of cover-letter comments in the schema"""
        self.cur.execute('ALTER TABLE series ADD COLUMN link')
        self.cur.execute('ALTER TABLE series ADD COLUMN cover_num_comments')

    def migrate_to(self, dest_version):
        while True:
            version = self.get_schema_version()
            if version == dest_version:
                break

            version += 1
            tout.info(f'Update database to v{version}')
            if version == 1:
                self._migrate_to_v1()
            elif version == 2:
                self._migrate_to_v2()
            self.cur.execute('DELETE FROM schema_version')
            self.cur.execute('INSERT INTO schema_version (version) VALUES (?)',
                             (version,))
            self.con.commit()

    def get_schema_version(self):
        version = 0
        try:
            # If there is no schema, assume v0
            self.cur.execute('SELECT version FROM schema_version')
            version = self.cur.fetchone()[0]
        except sqlite3.OperationalError:
            version = 0
        return version

    def ensure_exists(self):
        # If there is no database at all, create one
        try:
            self.cur.execute('SELECT name FROM series')
        except sqlite3.OperationalError:
            self.create_v0()

    def commit(self):
        self.con.commit()

    def rollback(self):
        self.con.rollback()

    def close(self):
        self.con.close()
        self.cur = None
        self.con = None

    def execute(self, query, args=None):
        if args:
            res = self.cur.execute(query, args)
        else:
            res = self.cur.execute(query)
        return res

    def lastrowid(self):
        return self.cur.lastrowid

    def rowcount(self):
        return self.cur.rowcount
