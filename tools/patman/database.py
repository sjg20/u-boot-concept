# SPDX-License-Identifier: GPL-2.0+
#
# Copyright 2025 Simon Glass <sjg@chromium.org>
#
"""Handles the patman database

This uses sqlite3 with a local file.

To adjsut the schema, increment LATEST, create a migrate_to_v<x>() function
and write some code in migrate_to() to call it.
"""

from collections import namedtuple, OrderedDict
import os
import sqlite3

from u_boot_pylib import tools
from u_boot_pylib import tout
from patman.series import Series

# Schema version (version 0 means there is no database yet)
LATEST = 3

# Information about a series/version record
SER_VER = namedtuple(
    'ser_ver',
    'idnum,series_id,version,link,cover_id,cover_num_comments,name')


class Database:
    """Database of information used by patman"""

    # dict of databases:
    #   key: filename
    #   value: Database object
    instances = {}

    def __init__(self, db_path):
        """Set up a new database object

        Args:
            db_path (str): Path to the database
        """
        if db_path in Database.instances:
            # Two connections to the database can cause:
            # sqlite3.OperationalError: database is locked
            raise ValueError(f"There is already a database for '{db_path}'")
        self.con = None
        self.cur = None
        self.db_path = db_path
        self.is_open = False
        Database.instances[db_path] = self

    @staticmethod
    def get_instance(db_path):
        """Get the database instance for a path

        This is provides to ensure that different callers can obtain the
        same database object when accessing the same database file.

        Args:
            db_path (str): Path to the database

        Return:
            Database: Database instance, which is created if necessary
        """
        db = Database.instances.get(db_path)
        if db:
            return db, False
        return Database(db_path), True

    def start(self):
        """Open the database read for use, migrate to latest schema"""
        self.open_it()
        self.migrate_to(LATEST)

    def open_it(self):
        """Open the database, creating it if necessary"""
        if self.is_open:
            raise ValueError('Already open')
        if not os.path.exists(self.db_path):
            tout.warning(f'Creating new database {self.db_path}')
        self.con = sqlite3.connect(self.db_path, autocommit=False)
        self.cur = self.con.cursor()
        self.is_open = True

    def close(self):
        """Close the database"""
        if not self.is_open:
            raise ValueError('Already closed')
        self.con.close()
        self.cur = None
        self.con = None
        self.is_open = False

    def create_v1(self):
        """Create a database with the v1 schema"""
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

    def _migrate_to_v2(self):
        """Add a schema_version table"""
        self.cur.execute('CREATE TABLE schema_version (version INTEGER)')

    def _migrate_to_v3(self):
        """Store the number of cover-letter comments in the schema"""
        self.cur.execute('ALTER TABLE ser_ver ADD COLUMN cover_id')
        self.cur.execute('ALTER TABLE ser_ver ADD COLUMN cover_num_comments INTEGER')
        self.cur.execute('ALTER TABLE ser_ver ADD COLUMN name')

    def migrate_to(self, dest_version):
        """Migrate the database to the selected version

        Args:
            dest_version (int): Version to migrate to
        """
        while True:
            version = self.get_schema_version()
            if version == dest_version:
                break

            self.close()
            tools.write_file(f'{self.db_path}old.v{version}',
                             tools.read_file(self.db_path))

            version += 1
            tout.info(f'Update database to v{version}')
            self.open_it()
            if version == 1:
                self.create_v1()
            elif version == 2:
                self._migrate_to_v2()
            elif version == 3:
                self._migrate_to_v3()

            # Save the new version if we have a schema_version table
            if version > 1:
                self.cur.execute('DELETE FROM schema_version')
                self.cur.execute('INSERT INTO schema_version (version) VALUES (?)',
                                 (version,))
            self.commit()

    def get_schema_version(self):
        """Get the version of the database's schema

        Return:
            int: Database version, 0 means there is no data; anything less than
                LATEST means the schema is out of date and must be updated
        """
        # If there is no database at all, assume v0
        version = 0
        try:
            self.cur.execute('SELECT name FROM series')
        except sqlite3.OperationalError:
            return 0

        # If there is no schema, assume v1
        try:
            self.cur.execute('SELECT version FROM schema_version')
            version = self.cur.fetchone()[0]
        except sqlite3.OperationalError:
            return 1
        return version

    def execute(self, query, parameters=()):
        """Execute a database query

        Args:
            query (str): Query string
            parameters (list of values): Parameters to pass

        Return:

        """
        return self.cur.execute(query, parameters)

    def commit(self):
        """Commit changes to the database"""
        self.con.commit()

    def rollback(self):
        """Roll back changes to the database"""
        self.con.rollback()

    def lastrowid(self):
        """Get the last row-ID reported by the database

        Return:
            int: Value for lastrowid
        """
        return self.cur.lastrowid

    def rowcount(self):
        """Get the row-count reported by the database

        Return:
            int: Value for rowcount
        """
        return self.cur.rowcount

    def _get_series_list(self, include_archived):
        """Get a list of Series objects from the database

        Args:
            include_archived (bool): True to include archives series

        Return:
            list of Series
        """
        res = self.execute(
            'SELECT id, name, desc FROM series ' +
            ('WHERE archived = 0' if not include_archived else ''))
        return [Series.from_fields(idnum=idnum, name=name, desc=desc)
                for idnum, name, desc in res.fetchall()]

    # series functions

    def series_add(self, name, desc):
        """Add a new series record

        The new record is set to not archived

        Args:
            name (str): Series name
            desc (str): Series description

        Return:
            int: ID num of the new series record
        """
        self.execute(
            'INSERT INTO series (name, desc, archived) '
            f"VALUES ('{name}', '{desc}', 0)")
        return self.lastrowid()

    def series_remove(self, idnum, dry_run=False):
        """Remove a series from the database

        The series must exist

        Args:
            idnum (int): ID num of series to remove
        """
        self.execute('DELETE FROM series WHERE id = ?', (idnum,))
        assert self.rowcount() == 1

    def series_remove_by_name(self, name, dry_run=False):
        """Remove a series from the database

        Args:
            name (str): Name of series to remove

        Raises:
            ValueError: Series does not exist (database is rolled back)
        """
        self.execute('DELETE FROM series WHERE name = ?', (name,))
        if self.rowcount() != 1:
            self.rollback()
            raise ValueError(f"No such series '{name}'")

    def series_get_dict(self, include_archived=False):
        """Get a dict of Series objects from the database

        Args:
            include_archived (bool): True to include archives series

        Return:
            OrderedDict:
                key: series name
                value: Series with idnum, name and desc filled out
        """
        sdict = OrderedDict()
        for ser in self._get_series_list(include_archived):
            sdict[ser.name] = ser
        return sdict

    def series_get_dict_by_id(self, include_archived=False):
        """Get a dict of Series objects from the database

        Return:
            OrderedDict:
                key: series ID
                value: Series with idnum, name and desc filled out
        """
        sdict = OrderedDict()
        for ser in self._get_series_list(include_archived):
            sdict[ser.idnum] = ser
        return sdict

    def series_find_by_name(self, name):
        """Find a series and return its details

        Args:
            name (str): Name to search for

        Returns:
            idnum, or None if not found
        """
        res = self.execute(
            'SELECT id FROM series WHERE '
            f"name = '{name}' AND archived = 0")
        recs = res.fetchall()
        if len(recs) != 1:
            return None
        return recs[0][0]

    def series_set_archived(self, series_idnum, archived):
        """Update archive flag for a series

        Args:
            series_idnum (int): ID num of the series
            archived (bool): Whether to mark the series as archived or
                unarchived
        """
        self.execute(
            'UPDATE series SET archived = ? WHERE id = ?',
            (archived, series_idnum))

    def series_set_name(self, series_idnum, name):
        """Update name for a series

        Args:
            series_idnum (int): ID num of the series
            name (str): new name to use
        """
        self.execute(
            'UPDATE series SET name = ? WHERE id = ?', (name, series_idnum))

    # ser_ver functions

    def ser_ver_get_link(self, series_idnum, version):
        """Get the link for a series version

        Args:
            series_idnum (int): ID num of the series
            version (int): Version number to search for

        Return:
            str: Patchwork link as a string, e.g. '12325', or None if none

        Raises:
            ValueError: Multiple matches are found
        """
        res = self.execute(
            'SELECT link FROM ser_ver WHERE '
            f"series_id = {series_idnum} AND version = '{version}'")
        recs = res.fetchall()
        if not recs:
            return None
        if len(recs) > 1:
            raise ValueError('Expected one match, but multiple matches found')
        return recs[0][0]

    def ser_ver_add(self, series_idnum, version, link=None):
        """Add a new ser_ver record

        Args:
            series_idnum (int): ID num of the series which is getting a new
                version
            version (int): Version number to add
            link (str): Patchwork link, or None if not known

        Return:
            int: ID num of the new ser_ver record
        """
        self.execute(
            'INSERT INTO ser_ver (series_id, version, link) VALUES (?, ?, ?)',
            (series_idnum, version, link))
        return self.lastrowid()

    def ser_ver_get_for_series(self, series_idnum, version):
        """Get a list of ser_ver records for a given series ID

        Args:
            series_idnum (int): ID num of the series to search
            version (int): Version number to search for

        Return: tuple:
            int: record id
            str: link
            str: cover_id
            int: cover_num_comments
            str: cover-letter name

        Raises:
            ValueError: There is no matching idnum/version
        """
        res = self.execute(
            'SELECT id, link, cover_id, cover_num_comments, name FROM ser_ver '
            'WHERE series_id = ? AND version = ?', (series_idnum, version))
        recs = res.fetchall()
        if not recs:
            raise ValueError(
                f'No matching series for id {series_idnum} version {version}')
        return recs[0]

    def ser_ver_get_ids_for_series(self, series_idnum, version=None):
        """Get a list of ser_ver records for a given series ID

        Args:
            series_idnum (int): ID num of the series to search
            version (int): Version number to search for, or None for all

        Return:
            list of int: List of svids for the matching records
        """
        if version:
            res = self.execute(
                'SELECT id FROM ser_ver WHERE series_id = ? AND version = ?',
                (series_idnum, version))
        else:
            res = self.execute(
                'SELECT id FROM ser_ver WHERE series_id = ?', (series_idnum,))
        return [i for i in res.fetchall()[0]]

    def ser_ver_get_list(self):
        """Get a list of patchwork entries from the database

        Return:
            list of SER_VER
        """
        res = self.execute(
            'SELECT id, series_id, version, link, cover_id, '
            'cover_num_comments,name FROM ser_ver')
        items = res.fetchall()
        return [SER_VER(*x) for x in items]

    def ser_ver_remove(self, series_idnum, version=None, remove_pcommits=True,
                       remove_series=True):
        """Delete a ser_ver record

        Removes the record which has the given series ID num and version

        Args:
            series_idnum (int): ID num of the series
            version (int): Version number, or None to remove all versions
            remove_pcommits (bool): True to remove associated pcommits too
            remove_series (bool): True to remove the series if versions is None
        """
        if remove_pcommits:
            # Figure out svids to delete
            svids = self.ser_ver_get_ids_for_series(series_idnum, version)

            self.pcommit_delete_list(svids)

        if version:
            self.execute(
                'DELETE FROM ser_ver WHERE series_id = ? AND version = ?',
                (series_idnum, version))
        else:
            self.execute(
                'DELETE FROM ser_ver WHERE series_id = ?',
                (series_idnum,))
        if not version and remove_series:
            self.series_remove(series_idnum)

    # pcommit functions

    def pcommit_add_list(self, svid, pcommits):
        """Add records to the pcommit table

        Args:
            svid (int): ser_ver ID num
            pcommits (list of PCOMMIT)
        """
        for pcm in pcommits:
            self.execute(
                'INSERT INTO pcommit (svid, seq, subject, change_id) VALUES '
                '(?, ?, ?, ?)', (svid, pcm.seq, pcm.subject, pcm.change_id))

    def pcommit_delete(self, svid):
        """Delete pcommit records for a given ser_ver ID

        Args_:
            svid (int): ser_ver ID num of records to delete
        """
        self.execute('DELETE FROM pcommit WHERE svid = ?', (svid,))

    def pcommit_delete_list(self, svid_list):
        """Delete pcommit records for a given set of ser_ver IDs

        Args_:
            svid (list int): ser_ver ID nums of records to delete
        """
        vals = ', '.join([str(x) for x in svid_list])
        self.execute('DELETE FROM pcommit WHERE svid IN (?)', (vals,))

    # upstream functions

    def upstream_add(self, name, url):
        """Add a new upstream record

        Args:
            name (str): Name of the tree
            url (str): URL for the tree

        Raises:
            ValueError if the name already exists in the database
        """
        try:
            self.execute(
                'INSERT INTO upstream (name, url) VALUES (?, ?)', (name, url))
        except sqlite3.IntegrityError as exc:
            if 'UNIQUE constraint failed: upstream.name' in str(exc):
                raise ValueError(f"Upstream '{name}' already exists") from exc

    def upstream_set_default(self, name):
        """Mark (only) the given upstream as the default

        Args:
            name (str): Name of the upstream remote to set as default, or None

        Raises:
            ValueError if more than one name matches (should not happen);
                database is rolled back
        """
        self.execute("UPDATE upstream SET is_default = 0")
        if name is not None:
            self.execute(
                'UPDATE upstream SET is_default = 1 WHERE name = ?', (name,))
            if self.rowcount() != 1:
                self.rollback()
                raise ValueError(f"No such upstream '{name}'")

    def upstream_get_default(self):
        """Get the name of the default upstream

        Return:
            str: Default-upstream name, or None if there is no default
        """
        res = self.execute(
            "SELECT name FROM upstream WHERE is_default = 1")
        recs = res.fetchall()
        if len(recs) != 1:
            return None
        return recs[0][0]

    def upstream_delete(self, name):
        """Delete an upstream target

        Args:
            name (str): Name of the upstream remote to delete

        Raises:
            ValueError: Upstream does not exist (database is rolled back)
        """
        self.execute(f"DELETE FROM upstream WHERE name = '{name}'")
        if self.rowcount() != 1:
            self.rollback()
            raise ValueError(f"No such upstream '{name}'")

    # settings functions

    def settings_update(self, name, proj_id, link_name):
        """Set the patchwork settings of the project

        Args:
            name (str): Name of the project to use in patchwork
            proj_id (int): Project ID for the project
            link_name (str): Link name for the project
        """
        self.execute('DELETE FROM settings')
        self.execute(
                'INSERT INTO settings (name, proj_id, link_name) '
                'VALUES (?, ?, ?)', (name, proj_id, link_name))

    def settings_get(self):
        """Get the patchwork settings of the project

        Returns:
            tuple or None if there are no settings:
                name (str): Project name, e.g. 'U-Boot'
                proj_id (int): Patchworks project ID for this project
                link_name (str): Patchwork's link-name for the project
        """
        res = self.execute("SELECT name, proj_id, link_name FROM settings")
        recs = res.fetchall()
        if len(recs) != 1:
            return None
        return recs[0]
