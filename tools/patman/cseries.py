# SPDX-License-Identifier: GPL-2.0+
#
# Copyright 2025 Google LLC
#
"""Handles the 'series' subcommand
"""

import asyncio
from collections import OrderedDict, defaultdict, namedtuple
import hashlib
import os
import re
import sqlite3
import sys
import time
from types import SimpleNamespace

import aiohttp
import pygit2
from pygit2.enums import CheckoutStrategy

from u_boot_pylib import cros_subprocess
from u_boot_pylib import gitutil
from u_boot_pylib import terminal
from u_boot_pylib import tout

from patman import patchstream
from patman.database import Database
from patman import patchwork
from patman import send
from patman.series import Series
from patman import status


# Tag to use for Change IDs
CHANGE_ID_TAG = 'Change-Id'

# Length of hash to display
HASH_LEN = 10

# Record from the pcommit table:
# id (int): record ID
# seq (int): Patch sequence in series (0 is first)
# subject (str): patch subject
# svid (int): ID of series/version record in ser_ver table
# change_id (str): Change-ID value
# status (str): Current status in patchwork
# patch_id (int): Patchwork's patch ID for this patch
# num_comments (int): Number of comments attached to the commit
PCOMMIT = namedtuple(
    'pcommit',
    'id,seq,subject,svid,change_id,state,patch_id,num_comments')

# Shorter version of some states, to save horizontal space
SHORTEN_STATE = {
    'handled-elsewhere': 'elsewhere',
    'awaiting-upstream': 'awaiting',
    'not-applicable': 'n/a',
    'changes-requested': 'changes',
}

# Information about a series/version record
SER_VER = namedtuple(
    'ser_ver',
    'idnum,series_id,version,link,cover_id,cover_num_comments,name')

# Summary info returned from Cseries.autolink_all()
AUTOLINK = namedtuple('autolink', 'name,version,link,desc,result')

def oid(oid_val):
    """Convert a string into a shortened hash

    The number of hex digits git uses for showing hashes depends on the size of
    the repo. For the purposes of showing hashes to the user in lists, we use a
    fixed value for now

    Args:
        str or Pygit2.oid: Hash value to shorten

    Return:
        str: Shortened hash
    """
    return str(oid_val)[:HASH_LEN]


def split_name_version(in_name):
    """Split a branch name into its series name and its version

    For example:
        'series' returns ('series', 1)
        'series3' returns ('series', 3)
    Args:
        in_name (str): Name to parse

    Return:
        tuple:
            str: series name
            int: series version, or None if there is none in in_name
    """
    m_ver = re.match(r'([^0-9]*)(\d*)', in_name)
    version = None
    if m_ver:
        name = m_ver.group(1)
        if m_ver.group(2):
            version = int(m_ver.group(2))
    else:
        name = in_name
    return name, version


class Cseries:
    """Database with information about series

    This class handles database read/write as well as operations in a git
    directory to update series information.
    """
    def __init__(self, topdir=None, colour=terminal.COLOR_IF_TERMINAL):
        """Set up a new Cseries

        Args:
            topdir (str): Top-level directory of the repo
            colour (terminal.enum): Whether to enable ANSI colour or not

        Properties:
            gitdir (str): Git directory (typically topdir + '/.git')
            db (Database): Database handler
            col (terminal.Colour): Colour object
            _fake_time (float): Holds the current fake time for tests, in
                seconds
            _fake_sleep (func): Function provided by a test; called to fake a
                'time.sleep()' call and take whatever action it wants to take.
                The only argument is the (Float) time to sleep for; it returns
                nothing
            loop (asyncio event loop): Loop used for Patchwork operations
        """
        self.topdir = topdir
        self.gitdir = None
        self.db = None
        self.col = terminal.Color(colour)
        self._fake_time = None
        self._fake_sleep = None
        self.loop = asyncio.get_event_loop()

    def open_database(self):
        """Open the database ready for use"""
        if not self.topdir:
            self.topdir = gitutil.get_top_level()
            if not self.topdir:
                raise ValueError('No git repo detected in current directory')
        self.gitdir = os.path.join(self.topdir, '.git')
        fname = f'{self.topdir}/.patman.db'

        # For the first instance, start it up with the expected schema
        self.db, is_new = Database.get_instance(fname)
        if is_new:
            self.db.start()
        else:
            # If a previous test has already checked the schema, just open it
            self.db.open_it()

    def close_database(self):
        """Close the database"""
        if self.db:
            self.db.close()

    def commit(self):
        """Commit changes to the database"""
        self.db.commit()

    def rollback(self):
        """Roll back changes to the database"""
        self.db.rollback()

    def lastrowid(self):
        """Get the last row-ID reported by the database

        Return:
            int: Value for lastrowid
        """
        return self.db.lastrowid()

    def rowcount(self):
        """Get the row-count reported by the database

        Return:
            int: Value for rowcount
        """
        return self.db.rowcount()

    def set_fake_time(self, fake_sleep):
        """Setup the fake timer

        Args:
            fake_sleep (func(float)): Function to call to fake a sleep
        """
        self._fake_time = 0
        self._fake_sleep = fake_sleep

    def inc_fake_time(self, inc_s):
        """Increment the fake time

        Args:
            inc_s (float): Amount to increment the fake time by
        """
        self._fake_time += inc_s

    def get_time(self):
        """Get the current time, fake or real

        This function should always be used to read the time so that faking the
        time works correctly in tests.

        Return:
            float: Fake time, if time is being faked, else real time
        """
        if self._fake_time is not None:
            return self._fake_time
        return time.monotonic()

    def sleep(self, time_s):
        """Sleep for a while

        This function should always be used to sleep so that faking the time
        works correctly in tests.

        Args:
            time_s (float): Amount of seconds to sleep for
        """
        print(f'Sleeping for {time_s} seconds')
        if self._fake_time is not None:
            self._fake_sleep(time_s)
        else:
            time.sleep(time_s)

    def get_series_dict(self, include_archived=False):
        """Get a dict of Series objects from the database

        Args:
            include_archived (bool): True to include archives series

        Return:
            OrderedDict:
                key: series name
                value: Series with idnum, name and desc filled out
        """
        res = self.db.execute('SELECT id, name, desc FROM series ' +
            ('WHERE archived = 0' if not include_archived else ''))
        sdict = OrderedDict()
        for idnum, name, desc in res.fetchall():
            ser = Series()
            ser.idnum = idnum
            ser.name = name
            ser.desc = desc
            sdict[name] = ser
        return sdict

    def get_series_dict_by_id(self, include_archived=False):
        """Get a dict of Series objects from the database

        Return:
            OrderedDict:
                key: series ID
                value: Series with idnum, name and desc filled out
        """
        res = self.db.execute('SELECT id, name, desc FROM series ' +
            ('WHERE archived = 0' if not include_archived else ''))
        sdict = OrderedDict()
        for idnum, name, desc in res.fetchall():
            ser = Series()
            ser.idnum = idnum
            ser.name = name
            ser.desc = desc
            sdict[idnum] = ser
        return sdict

    def get_ser_ver_list(self):
        """Get a list of patchwork entries from the database

        Return:
            list of SER_VER
        """
        res = self.db.execute(
            'SELECT id, series_id, version, link, cover_id, cover_num_comments, '
            'name FROM ser_ver')
        items = res.fetchall()
        return [SER_VER(*x) for x in items]

    def get_ser_ver_dict(self):
        """Get a dict of patchwork entries from the database

        Return: dict contain all records:
            key (int): ser_ver id
            value (SER_VER): Information about one ser_ver record
        """
        svlist = self.get_ser_ver_list()
        svdict = {}
        for sver in svlist:
            svdict[sver.idnum] = sver
        return svdict

    def get_upstream_dict(self):
        """Get a list of upstream entries from the database

        Return:
            OrderedDict:
                key (str): upstream name
                value (str): url
        """
        res = self.db.execute('SELECT name, url, is_default FROM upstream')
        udict = OrderedDict()
        for name, url, is_default in res.fetchall():
            udict[name] = url, is_default
        return udict

    def get_pcommit_dict(self, find_svid=None):
        """Get a dict of pcommits entries from the database

        Args:
            find_svid (int): If not None, finds the records associated with a
                particular series and version

        Return:
            OrderedDict:
                key (int): record ID if find_svid is None, else seq
                value (PCOMMIT): record data
        """
        query = ('SELECT id, seq, subject, svid, change_id, state, patch_id, '
                 'num_comments FROM pcommit')
        if find_svid is not None:
            query += f' WHERE svid = {find_svid}'
        res = self.db.execute(query)
        pcdict = OrderedDict()
        for (idnum, seq, subject, svid, change_id, state, patch_id,
             num_comments) in res.fetchall():
            pc = PCOMMIT(idnum, seq, subject, svid, change_id, state, patch_id,
                         num_comments)
            if find_svid is not None:
                pcdict[seq] = pc
            else:
                pcdict[idnum] = pc
        return pcdict

    def get_series_info(self, idnum):
        """Get information for a series from the database

        Args:
            idnum (int): Series ID to look up
        Return: tuple:
            str: Series name
            str: Series description
        """
        res = self.db.execute('SELECT name, desc FROM series WHERE id = ?',
                              (idnum,))
        recs = res.fetchall()
        if len(recs) != 1:
            raise ValueError(f'No series found (id {idnum} len {len(recs)})')
        return recs[0]

    def _prep_series(self, name, end=None):
        """Prepare to work with a series

        Args:
            name (str): Branch name with version appended, e.g. 'fix2'

        Return: tuple:
            str: Series name, e.g. 'fix'
            Series: Collected series information, including name
            int: Version number, e.g. 2
            str: Message to show
        """
        ser, version = self.parse_series_and_version(name, None)
        if not name:
            name = self.get_branch_name(ser.name, version)

        # First check we have a branch with this name
        if not gitutil.check_branch(name, git_dir=self.gitdir):
            raise ValueError(f"No branch named '{name}'")

        count = gitutil.count_commits_to_branch(name, self.gitdir, end)
        if not count:
            raise ValueError(
                'Cannot detect branch automatically: Perhaps use -U <upstream-commit> ?')

        series = patchstream.get_metadata(name, 0, count, git_dir=self.gitdir)
        self.copy_db_fields_to(series, ser)
        msg = None
        if end:
            repo = pygit2.init_repository(self.gitdir)
            target = repo.revparse_single(end)
            first_line = target.message.splitlines()[0]
            msg = f'Ending before {oid(target.id)} {first_line}'

        return name, series, version, msg

    def copy_db_fields_to(self, series, in_series):
        """Copy over fields used by Cseries from one series to another

        This copes desc, idnum and name

        Args:
            series (Series): Series to copy to
            in_series (Series): Series to copy from
        """
        series.desc = in_series.desc
        series.idnum = in_series.idnum
        series.name = in_series.name

    def _handle_mark(self, branch_name, in_series, version, mark,
                     allow_unmarked, force_version, dry_run):
        """Handle marking a series, checking for unmarked commits, etc.

        Args:
            branch_name (str): Name of branch to sync, or None for current one
            in_series (Series): Series object
            version (int): branch version, e.g. 2 for 'mychange2'
            mark (bool): True to mark each commit with a change ID
            allow_unmarked (str): True to not require each commit to be marked
            force_version (bool): True if ignore a Series-version tag that
                doesn't match its branch name
            dry_run (bool): True to do a dry run

        Returns:
            Series: New series object, if the series was marked;
                copy_db_fields_to() is used to copy fields over

        Raises:
            ValueError: Series being unmarked when it should be marked, etc.
        """
        series = in_series
        if 'version' in series and int(series.version) != version:
            msg = (f"Series name '{branch_name}' suggests version {version} "
                   f"but Series-version tag indicates {series.version}")
            if not force_version:
                raise ValueError(msg + ' (see --force-version)')

            tout.warning(msg)
            tout.warning(f'Updating Series-version tag to version {version}')
            self.update_series(branch_name, series, int(series.version),
                               new_name=None, dry_run=dry_run, add_vers=version)

            # Collect the commits again, as the hashes have changed
            series = patchstream.get_metadata(branch_name, 0,
                                              len(series.commits),
                                              git_dir=self.gitdir)
            self.copy_db_fields_to(series, in_series)

        if mark:
            add_oid = self._mark_series(branch_name, series, dry_run=dry_run)

            # Collect the commits again, as the hashes have changed
            series = patchstream.get_metadata(add_oid, 0, len(series.commits),
                                              git_dir=self.gitdir)
            self.copy_db_fields_to(series, in_series)

        bad_count = 0
        for commit in series.commits:
            if not commit.change_id:
                bad_count += 1
        if bad_count and not allow_unmarked:
            raise ValueError(
                f'{bad_count} commit(s) are unmarked; please use -m or -M')

        return series

    def add_series(self, branch_name, desc=None, mark=False,
                   allow_unmarked=False, end=None, force_version=False,
                   dry_run=False):
        """Add a series to the database

        Args:
            branch_name (str): Name of branch to sync, or None for current one
            desc (str): Description to use, or None to use the series subject
            mark (str): True to mark each commit with a change ID
            allow_unmarked (str): True to not require each commit to be marked
            end (str): Add only commits up to but exclu
            force_version (bool): True if ignore a Series-version tag that
                doesn't match its branch name
            dry_run (bool): True to do a dry run
        """
        name, ser, version, msg = self._prep_series(branch_name, end)
        tout.info(f"Adding series '{ser.name}' v{version}: mark {mark} "
                  f'allow_unmarked {allow_unmarked}')
        if msg:
            tout.info(msg)
        if desc is None:
            if not ser.cover:
                raise ValueError(
                    f"Branch '{name}' has no cover letter - please provide description")
            desc = ser.cover[0]

        ser = self._handle_mark(name, ser, version, mark, allow_unmarked,
                                force_version, dry_run)
        link = ser.get_link_for_version(version)

        msg = 'Added'
        added = False
        series_id = self.find_series_by_name(ser.name)
        if not series_id:
            self.db.execute(
                'INSERT INTO series (name, desc, archived) '
                f"VALUES ('{ser.name}', '{desc}', 0)")
            series_id = self.lastrowid()
            added = True
            msg += f" series '{ser.name}'"

        if version not in self.get_version_list(series_id):
            self.db.execute(
                'INSERT INTO ser_ver (series_id, version, link) VALUES '
                '(?, ?, ?)', (series_id, version, link))
            svid = self.lastrowid()
            msg += f" v{version}"
            if not added:
                msg += f" to existing series '{ser.name}'"
            added = True

            self.add_series_commits(ser, svid)
            count = len(ser.commits)
            msg += f" ({count} commit{'s' if count > 1 else ''})"
        if not added:
            tout.info(f"Series '{ser.name}' v{version} already exists")
            msg = None
        elif not dry_run:
            self.commit()
        else:
            self.rollback()
            series_id = None
        ser.desc = desc
        ser.idnum = series_id

        if msg:
            tout.info(msg)
        if dry_run:
            tout.info('Dry run completed')

    def add_series_commits(self, series, svid):
        """Add a commits from a series into the database

        Args:
            series (Series): Series containing commits to add
            svid (int): ser_ver-table ID to use for each commit
        """
        for seq, commit in enumerate(series.commits):
            self.db.execute(
                'INSERT INTO pcommit (svid, seq, subject, change_id) '
                'VALUES (?, ?, ?, ?)',
                (str(svid), seq, commit.subject, commit.change_id))

    def get_series_by_name(self, name):
        """Get a Series object from the database by name

        Args:
            name (str): Name of series to get

        Return:
            Series: Object containing series info, or None if none
        """
        res = self.db.execute(
            f"SELECT id, name, desc FROM series WHERE name = '{name}'")
        recs = res.fetchall()
        if not recs:
            return None
        if len(recs) > 1:
            raise ValueError('Expected one match, but multiple matches found')
        ser = Series()
        ser.idnum, ser.name, ser.desc = recs[0]
        return ser

    def get_branch_name(self, name, version):
        """Get the branch name for a particular version

        Args:
            name (str): Base name of branch
            version (int): Version number to use
        """
        return name + (f'{version}' if version > 1 else '')

    def ensure_version(self, ser, version):
        """Ensure that a version exists in a series

        Args:
            ser (Series): Series information, with idnum and name used here
            version (int): Version to check

        Returns:
            list of int: List of versions
        """
        versions = self.get_version_list(ser.idnum)
        if version not in versions:
            raise ValueError(
                f"Series '{ser.name}' does not have a version {version}")
        return versions

    def _set_link(self, ser_id, name, version, link, update_commit,
                  dry_run=False):
        """Add / update a series-links link for a series

        Args:
            ser_id (int): Series ID number
            name (str): Series name (used to find the branch)
            version (int): Version number (used to update the database)
            link (str): Patchwork link-string for the series
            update_commit (bool): True to update the current commit with the
                link
            dry_run (bool): True to do a dry run

        Return:
            bool: True if the database was update, False if the ser_id or
                version was not found
        """
        if update_commit:
            branch_name = self.get_branch_name(name, version)
            _, ser, max_vers, _ = self._prep_series(branch_name)
            self.update_series(branch_name, ser, max_vers, add_vers=version,
                               dry_run=dry_run, add_link=link)
        if link is None:
            link = ''
        self.db.execute(
            f"UPDATE ser_ver SET link = '{link}' WHERE "
            'series_id = ? AND version = ?', (ser_id, version))
        updated = self.rowcount() != 0
        if dry_run:
            self.rollback()
        else:
            self.commit()

        return updated

    def set_link(self, series_name, version, link, update_commit):
        """Add / update a series-links link for a series

        Args:
            series_name (str): Name of series to use, or None to use current
                branch
            version (int): Version number, or None to detect from name
            link (str): Patchwork link-string for the series
            update_commit (bool): True to update the current commit with the
                link
        """
        ser, version = self.parse_series_and_version(series_name, version)
        self.ensure_version(ser, version)

        self._set_link(ser.idnum, ser.name, version, link, update_commit)
        self.commit()
        tout.info(f"Setting link for series '{ser.name}' v{version} to {link}")

    def get_link(self, series, version):
        """Get the patchwork link for a version of a series

        Args:
            series (str): Name of series to use, or None to use current branch
            version (int): Version number or None for current

        Return:
            str: Patchwork link as a string, e.g. '12325'
        """
        ser, version = self.parse_series_and_version(series, version)
        self.ensure_version(ser, version)

        res = self.db.execute('SELECT link FROM ser_ver WHERE '
            f"series_id = {ser.idnum} AND version = '{version}'")
        recs = res.fetchall()
        if not recs:
            return None
        if len(recs) > 1:
            raise ValueError('Expected one match, but multiple matches found')
        return recs[0][0]

    def search_link(self, pwork, series, version):
        """Search patch for the link for a series

        Returns either the single match, or None, in which case the second part
        of the tuple is filled in

        Args:
            pwork (Patchwork): Patchwork object to use
            series (str): Series name to search for, or None for current series
                that is checked out
            version (int): Version to search for, or None for current version
                detected from branch name

        Returns:
            tuple:
                int: ID of the series found, or None
                list of possible matches, or None, each a dict:
                    'id': series ID
                    'name': series name
                str: series name
                int: series version
                str: series description
        """
        _, ser, version, _, _, _, _, _ = self._get_patches(series, version)

        if not ser.desc:
            raise ValueError(f"Series '{ser.name}' has an empty description")

        pws, options = self.loop.run_until_complete(pwork.find_series(
            ser, version))
        return pws, options, ser.name, version, ser.desc

    def autolink(self, pwork, series, version, update_commit, wait_s=0):
        """Automatically find a series link by looking in patchwork

        Args:
            pwork (Patchwork): Patchwork object to use
            series (str): Series name to search for, or None for current series
                that is checked out
            version (int): Version to search for, or None for current version
                detected from branch name
            update_commit (bool): True to update the current commit with the
                link
            wait_s (int): Number of seconds to wait for the autolink to succeed
        """
        start = self.get_time()
        stop = start + wait_s
        sleep_time = 20
        while True:
            pws, options, name, version, desc = self.search_link(
                pwork, series, version)
            if pws:
                if wait_s:
                    tout.info(f'Link completed after {self.get_time() - start} seconds')
                break

            print(f"Possible matches for '{name}' v{version} desc '{desc}':")
            print('  Link  Version  Description')
            for opt in options:
                print(f"{opt['id']:6}  {opt['version']:7}  {opt['name']}")
            if not wait_s or self.get_time() > stop:
                delay = f' after {wait_s} seconds' if wait_s else ''
                raise ValueError(f"Cannot find series '{desc}{delay}'")

            self.sleep(sleep_time)

        self.set_link(name, version, pws, update_commit)

    def _get_autolink_dict(self, sdict, link_all_versions):
        """Get a dict of ser_vers to fetch, along with their patchwork links

        Note that this returns items that already have links, as well as those
        without links

        Args:
            sdict:
                key: series ID
                value: Series with idnum, name and desc filled out
            link_all_versions (bool): True to sync all versions of a series,
                False to sync only the latest version

        Return: tuple:
            dict:
                key (int): svid
                value (tuple):
                   int: series ID
                   str: series name
                   int: series version
                   str: patchwork link for the series, or None if none
                   desc: cover-letter name / series description
        """
        svdict = self.get_ser_ver_dict()
        to_fetch = {}

        if link_all_versions:
            for svid, ser_id, version, link, _, _, _ in \
                    self.get_ser_ver_list():
                ser = sdict[ser_id]

                pwc = self.get_pcommit_dict(svid)
                count = len(pwc)
                branch = self.join_name_version(ser.name, version)
                series = patchstream.get_metadata(branch, 0, count,
                                                  git_dir=self.gitdir)
                self.copy_db_fields_to(series, ser)

                to_fetch[svid] = ser_id, series.name, version, link, series
        else:
            # Find the maximum version for each series
            max_vers = self.series_all_max_versions()

            # Get a list of links to fetch
            for svid, ser_id, version in max_vers:
                svinfo = svdict[svid]
                ser = sdict[ser_id]

                pwc = self.get_pcommit_dict(svid)
                count = len(pwc)
                branch = self.join_name_version(ser.name, version)
                series = patchstream.get_metadata(branch, 0, count,
                                                  git_dir=self.gitdir)
                self.copy_db_fields_to(series, ser)

                to_fetch[svid] = (ser_id, series.name, version, svinfo.link,
                                  series)
        return to_fetch

    def autolink_all(self, pwork, update_commit, link_all_versions,
                     replace_existing, dry_run, show_summary=True):
        """Automatically find a series link by looking in patchwork

        Args:
            pwork (Patchwork): Patchwork object to use
            update_commit (bool): True to update the current commit with the
                link
            link_all_versions (bool): True to sync all versions of a series,
                False to sync only the latest version
            replace_existing (bool): True to sync a series even if it already
                has a link
            dry_run (bool): True to do a dry run
            show_summary (bool): True to show a summary of how things went

        Return:
            OrderedDict of summary info:
                key (int): ser_ver ID
                value (AUTOLINK): result of autolinking on this ser_ver
        """
        sdict = self.get_series_dict_by_id()
        all_ser_vers = self._get_autolink_dict(sdict, link_all_versions)

        # Get rid of things without a description
        valid = {}
        state = {}
        no_desc = 0
        not_found = 0
        updated = 0
        failed = 0
        already = 0
        for svid, (ser_id, name, version, link, desc) in all_ser_vers.items():
            if link and not replace_existing:
                state[svid] = f'already:{link}'
                already += 1
            elif desc:
                valid[svid] = ser_id, version, link, desc
            else:
                no_desc += 1
                state[svid] = 'missing description'

        results, requests = self.loop.run_until_complete(
            pwork.find_series_list(valid))

        for svid, ser_id, link, _ in results:
            if link:
                version = all_ser_vers[svid][2]
                if self._set_link(ser_id, sdict[ser_id].name, version,
                                  link, update_commit, dry_run=dry_run):
                    updated += 1
                    state[svid] = f'linked:{link}'
                else:
                    failed += 1
                    state[svid] = 'failed'
            else:
                not_found += 1
                state[svid] = 'not found'

        # Create a summary sorted by name and version
        summary = OrderedDict()
        for svid in sorted(all_ser_vers, key=lambda k: all_ser_vers[k][1:2]):
            _, name, version, link, ser = all_ser_vers[svid]
            summary[svid] = AUTOLINK(name, version, link, ser.desc, state[svid])

        if show_summary:
            msg = f'{updated} series linked'
            if already:
                msg += f', {already} already linked'
            if not_found:
                msg += f', {not_found} not found'
            if no_desc:
                msg += f', {no_desc} missing description'
            if failed:
                msg += f', {failed} updated failed'
            tout.info(msg + f' ({requests} requests)')

            tout.info('')
            tout.info(f"{'Name':15}  Version  {'Description':40}  Result")
            border = f"{'-' * 15}  -------  {'-' * 40}  {'-' * 15}"
            tout.info(border)
            for name, version, link, desc, state in summary.values():
                bright = True
                if state.startswith('already'):
                    col = self.col.GREEN
                    bright = False
                elif state.startswith('linked'):
                    col = self.col.MAGENTA
                else:
                    col = self.col.RED
                col_state = self.col.build(col, state, bright)
                tout.info(f"{name:16.16} {version:7}  {desc or '':40.40}  "
                          f'{col_state}')
            tout.info(border)
        if dry_run:
            tout.info('Dry run completed')

        return summary

    def get_version_list(self, idnum):
        """Get a list of the versions available for a series

        Args:
            idnum (int): ID of series to look up

        Return:
            str: List of versions
        """
        if idnum is None:
            raise ValueError('Unknown series idnum')
        res = self.db.execute('SELECT version FROM ser_ver WHERE '
            f"series_id = {idnum}")
        recs = res.fetchall()
        return [item[0] for item in recs]

    def join_name_version(self, in_name, version):
        """Convert a series name plus a version into a branch name

        For example:
            ('series', 1) returns 'series'
            ('series', 3) returns 'series3'

        Args:
            in_name (str): Series name
            version (int): Version number

        Return:
            str: associated branch name
        """
        if version == 1:
            return in_name
        return f'{in_name}{version}'

    def parse_series(self, name):
        """Parse the name of a series, or detect it from the current branch

        Args:
            name (str or None): name of series

        Return:
            Series: New object with the name set; idnum is also set if the
                series exists in the database
        """
        if not name:
            name = gitutil.get_branch(self.gitdir)
        name, _ = split_name_version(name)
        ser = self.get_series_by_name(name)
        if not ser:
            ser = Series()
            ser.name = name
        return ser

    def parse_series_and_version(self, in_name, in_version):
        """Parse the name and version of a series, or detect from current branch

        Figures out the name from in_name, or if that is None, from the current
            branch.

        Uses the version in_version, or if that is None, uses the int at the end
        of the name (e.g. 'series' is version 1, 'series4' is version 4)

        Args:
            in_name (str or None): name of series
            in_version (str or None): version of series

        Return:
            tuple:
                Series: New object with the name set; idnum is also set if the
                    series exists in the database
                int: Series version-number detected from the name
                    (e.g. 'fred' is version 1, 'fred2' is version 2)
        """
        name = in_name
        if not name:
            name = gitutil.get_branch(self.gitdir)
            if not name:
                raise ValueError('No branch detected: please use -s <series>')
        name, version = split_name_version(name)
        if not name:
            raise ValueError(f"Series name '{in_name}' cannot be a number, use '<name><version>'")
        if not version:
            version = in_version
        if in_version and version != in_version:
            raise ValueError(
                f"Version mismatch: -V has {in_version} but branch name indicates {version}")
        if not version:
            version = 1
        if version > 99:
            raise ValueError(f"Version {version} exceeds 99")
        ser = self.get_series_by_name(name)
        if not ser:
            ser = Series()
            ser.name = name
        return ser, version

    def set_archived(self, series, archived):
        """Set whether a series is archived or not

        Args:
            series (str): Name of series to use, or None to use current branch
            archived (bool): Whether to mark the series as archived or
                unarchived
        """
        ser = self.parse_series(series)
        if not ser.idnum:
            raise ValueError(f"Series '{ser.name}' not found in database")
        ser.archived = archived
        self.db.execute(
            f'UPDATE series SET archived = {int(archived)} WHERE '
            f'id = {ser.idnum}')
        self.commit()

    def series_get_version_stats(self, idnum, vers):
        """Get the stats for a series

        Args:
            idnum (int): ID number of series to process
            vers (int): Version number to process

        Return:
            tuple:
                str: Status string, '<accepted>/<count>'
                OrderedDict:
                    key (int): record ID if find_svid is None, else seq
                    value (PCOMMIT): record data
        """
        svid, link = self.get_series_svid_link(idnum, vers)
        pwc = self.get_pcommit_dict(svid)
        count = len(pwc.values())
        if link:
            accepted = 0
            for pcm in pwc.values():
                accepted += pcm.state == 'accepted'
        else:
            accepted = '-'
        return f'{accepted}/{count}', pwc

    def series_list(self):
        """List all series

        Lines all series along with their description, number of patches
        accepted and  the available versions
        """
        sdict = self.get_series_dict()
        print(f"{'Name':15}  {'Description':40}  Accepted  Versions")
        border = f"{'-' * 15}  {'-' * 40}  --------  {'-' * 15}"
        print(border)
        for name in sorted(sdict):
            ser = sdict[name]
            versions = self.get_version_list(ser.idnum)
            stat = self.series_get_version_stats(
                ser.idnum, self.series_max_version(ser.idnum))[0]

            vlist = ' '.join([str(ver) for ver in sorted(versions)])

            print(f'{name:16.16} {ser.desc:41.41} {stat.rjust(8)}  {vlist}')
        print(border)

    def update_series(self, name, series, max_vers, new_name=None,
                      dry_run=False, add_vers=None, add_link=None,
                      add_rtags=None):
        """Rewrite a series to update the Series-version/Series-links lines

        This updates the series in git; it does not update the database

        Args:
            name (str): Name of the branch to process
            series (Series): Series object
            max_vers (int): Version number of the series being updated
            new_name (str or None): New name, if a new branch is to be created
            dry_run (bool): True to do a dry run, restoring the original tree
                afterwards
            vers (int or None): Version number to add to the series, if any
            add_vers (int or None): Version number to add to the series, if any
            add_link (str or None): Link to add to the series, if any
            add_rtags (list of dict): List of review tags to add, one item for
                    each commit, each a dict:
                key: Response tag (e.g. 'Reviewed-by')
                value: Set of people who gave that response, each a name/email
                    string

        Return:
            pygit.oid: oid of the new branch
        """
        added_version = False
        added_link = False
        for vals in self._process_series(name, series, new_name, dry_run):
            out = []
            for line in vals.msg.splitlines():
                m_ver = re.match('Series-version:(.*)', line)
                m_links = re.match('Series-links:(.*)', line)
                if m_ver and add_vers:
                    if ('version' in series and
                        int(series.version) != max_vers):
                        tout.warning(
                            f'Branch {name}: Series-version tag '
                            f'{series.version} does not match expected version '
                            f'{max_vers}')
                    if add_vers:
                        if add_vers == 1:
                            vals.info += f'deleted version {add_vers} '
                        else:
                            vals.info += f'added version {add_vers} '
                            out.append(f'Series-version: {add_vers}')
                    added_version = True
                elif m_links and add_link is not None:
                    links = series.get_links(m_links.group(1), max_vers)
                    links[max_vers] = add_link
                    new_links = series.build_links(links)
                    vals.info += f"added links '{new_links}' "
                    out.append(f'Series-links: {new_links}')
                    added_link = True
                else:
                    out.append(line)
            if vals.final:
                if not added_version and add_vers and add_vers > 1:
                    vals.info += f'added version {add_vers} '
                    out.append(f'Series-version: {add_vers}')
                if not added_link and add_link:
                    new_links = f'{max_vers}:{add_link}'
                    vals.info += f"added links '{new_links}' "
                    out.append(f'Series-links: {new_links}')

            vals.msg = '\n'.join(out) + '\n'
            if add_rtags and add_rtags[vals.seq]:
                lines = []
                for tag, people in add_rtags[vals.seq].items():
                    for who in people:
                        lines.append(f'{tag}: {who}')
                vals.msg = patchstream.insert_tags(vals.msg.rstrip(),
                                                   sorted(lines))
                vals.info += (f'added {len(lines)} '
                              f"tag{'' if len(lines) == 1 else 's'}")

    def increment(self, series_name, dry_run=False):
        """Increment a series to the next version and create a new branch

        Args:
            series_name (str): Name of series to use, or None to use current
                branch
            dry_run (bool): True to do a dry run
        """
        ser = self.parse_series(series_name)
        if not ser.idnum:
            raise ValueError(f"Series '{ser.name}' not found in database")

        max_vers = self.series_max_version(ser.idnum)

        branch_name = self.get_branch_name(ser.name, max_vers)
        svid = self.get_series_svid(ser.idnum, max_vers)
        pwc = self.get_pcommit_dict(svid)
        count = len(pwc.values())
        series = patchstream.get_metadata(branch_name, 0, count,
                                          git_dir=self.gitdir)
        tout.info(f"Increment '{ser.name}' v{max_vers}: {count} patches")

        # Create a new branch
        vers = max_vers + 1
        new_name = self.join_name_version(ser.name, vers)

        self.update_series(ser.name, series, max_vers, new_name, dry_run,
                           add_vers=vers)

        old_svid = self.get_series_svid(ser.idnum, max_vers)
        pcd = self.get_pcommit_dict(old_svid)

        self.db.execute(
            'INSERT INTO ser_ver (series_id, version) VALUES (?, ?)',
            (ser.idnum, vers))
        svid = self.lastrowid()

        for pcm in pcd.values():
            self.db.execute(
                'INSERT INTO pcommit (svid, seq, subject, change_id) VALUES '
                '(?, ?, ?, ?)', (svid, pcm.seq, pcm.subject, pcm.change_id))

        if not dry_run:
            self.commit()
        else:
            self.rollback()

        # repo.head.set_target(amended)
        tout.info(f'Added new branch {new_name}')
        if dry_run:
            tout.info('Dry run completed')

    def decrement(self, series, dry_run=False):
        """Decrement a series to the previous version and delete the branch

        Args:
            series (str): Name of series to use, or None to use current branch
            dry_run (bool): True to do a dry run
        """
        ser = self.parse_series(series)
        if not ser.idnum:
            raise ValueError(f"Series '{ser.name}' not found in database")

        max_vers = self.series_max_version(ser.idnum)
        if max_vers < 2:
            raise ValueError(f"Series '{ser.name}' only has one version")

        tout.info(f"Removing series '{ser.name}' v{max_vers}")

        new_max = max_vers - 1

        repo = pygit2.init_repository(self.gitdir)
        if not dry_run:
            name = self.get_branch_name(ser.name, new_max)
            branch = repo.lookup_branch(name)
            repo.checkout(branch)

            del_name = f'{ser.name}{max_vers}'
            del_branch = repo.lookup_branch(del_name)
            branch_oid = del_branch.peel(pygit2.GIT_OBJ_COMMIT).oid
            del_branch.delete()
            print(f"Deleted branch '{del_name}' {oid(branch_oid)}")

        old_svid = self.get_series_svid(ser.idnum, max_vers)

        self.db.execute(
            'DELETE FROM ser_ver WHERE series_id = ? and version = ?',
            (ser.idnum, max_vers))
        self.db.execute(
            'DELETE FROM pcommit WHERE svid = ?', (old_svid,))
        if not dry_run:
            self.commit()
        else:
            self.rollback()

    def _prepare_process(self, name, count, new_name=None, quiet=False):
        """Get ready to process all commits in a branch

        Args:
            name (str): Name of the branch to process
            count (int): Number of commits
            new_name (str or None): New name, if a new branch is to be created
            quiet (bool): True to avoid output (used for testing)

        Return: tuple:
            repo (pygit2.repo): Repo to use
            pygit2.oid: Upstream commit, onto which commits should be added
            name (str): (Possibly new) name of branch to process
            Pygit2.branch: Original branch, for later use
        """
        upstream_name = gitutil.get_upstream(self.gitdir, name)[0]

        tout.debug(f"_process_series name '{name}' new_name '{new_name}' "
                   f"upstream_name '{upstream_name}'")
        dirty = gitutil.check_dirty(self.gitdir, self.topdir)
        if dirty:
            raise ValueError(
                f"Modified files exist: use 'git status' to check: {dirty[:5]}")
        repo = pygit2.init_repository(self.gitdir)

        commit = None
        try:
            upstream = repo.lookup_reference(upstream_name)
            upstream_name = upstream.name
            commit = upstream.peel(pygit2.GIT_OBJ_COMMIT)
        except KeyError:
            upstream_name = f'{name}~{count}'
            commit = repo.revparse_single(upstream_name)
        branch = repo.lookup_branch(name)

        if not quiet:
            tout.info(f"Checking out upstream commit {upstream_name}")
        if new_name:
            name = new_name

        # Check out the upstream commit (detached HEAD)
        repo.checkout_tree(commit, strategy=CheckoutStrategy.FORCE |
                           CheckoutStrategy.RECREATE_MISSING)
        repo.set_head(commit.oid)

        return repo, repo.head, branch, name

    def _pick_commit(self, repo, cmt):
        """Apply a commit to the source tree, without commiting it

        _prepare_process() must be called before starting to pick commits

        This function must be called before _finish_commit()

        Args:
            repo (pygit2.repo): Repo to use
            cmt (Commit): Commit to apply

        Return: tuple:
            tree_id (pygit2.oid): Oid of index with source-changes applied
            commit (pygit2.oid): Old commit being cherry-picked
        """
        tout.detail(f"- adding {oid(cmt.hash)} {cmt}")
        repo.cherrypick(cmt.hash)
        if repo.index.conflicts:
            raise ValueError('Conflicts detected')

        tree_id = repo.index.write_tree()
        cherry = repo.get(cmt.hash)
        tout.detail(f"cherry {oid(cherry.oid)}")
        return tree_id, cherry

    def _finish_commit(self, repo, tree_id, cherry, cur, msg=None):
        """Complete a commit

        This must be called after _pick_commit()

        Args:
            repo (pygit2.repo): Repo to use
            tree_id (pygit2.oid): Oid of index with source-changes applied
            cherry (commit): Commit object which holds the author and committter
            commit (pygit2.oid): Old commit being cherry-picked
            cur (pygit2.reference): Reference to parent to use for the commit
            msg (str): Commit subject and message, or None to use cherry.message
        """
        if msg is None:
            msg = cherry.message
        repo.create_commit('HEAD', cherry.author, cherry.committer,
                           msg, tree_id, [cur.target])
        return repo.head

    def _finish_process(self, repo, branch, name, new_name=None, dry_run=False,
                        quiet=False):
        """Finish processing commits

        Args:
            repo (pygit2.repo): Repo to use
            branch (pygit2.branch): Branch returned by _prepare_process()
            name (str): Name of the branch to process
            new_name (str or None): New name, if a new branch is being created
            dry_run (bool): True to do a dry run, restoring the original tree
                afterwards
            quiet (bool): True to avoid output (used for testing)

        Return:
            pygit2.reference: Final commit after everything is completed
        """
        repo.state_cleanup()

        # Update the branch
        target = repo.revparse_single('HEAD')
        if not quiet:
            tout.info(f"Updating branch {name} to {str(target.oid)[:HASH_LEN]}")
        if dry_run:
            if new_name:
                repo.checkout(branch.name)
            else:
                branch_oid = branch.peel(pygit2.GIT_OBJ_COMMIT).oid
                repo.checkout_tree(repo.get(branch_oid))
                repo.head.set_target(branch_oid)
        else:
            if new_name:
                new_branch = repo.branches.create(new_name, target)
                if branch.upstream:
                    new_branch.upstream = branch.upstream
                branch = new_branch
            else:
                branch.set_target(target.oid)
            repo.checkout(branch)
        return target

    def make_change_id(self, commit):
        """Make a Change ID for a commit

        This is similar to the gerrit script:
        git var GIT_COMMITTER_IDENT ; echo "$refhash" ; cat "README"; }
            | git hash-object --stdin)
        """
        sig = commit.committer
        val = hashlib.sha1()
        to_hash = f'{sig.name} <{sig.email}> {sig.time} {sig.offset}'
        val.update(to_hash.encode('utf-8'))
        val.update(str(commit.tree_id).encode('utf-8'))
        val.update(commit.message.encode('utf-8'))
        return val.hexdigest()

    def filter_commits(self, name, series, seq_to_drop):
        """Filter commits to drop one

        Args:
            name (str): Name of the branch to process
            series (Series): Series object
            seq_to_drop (int): Commit sequence to drop; commits are numbered
                from 0, which is the one after the upstream branch, to count - 1
        """
        count = len(series.commits)
        repo, cur, branch, name = self._prepare_process(name, count, quiet=True)
        for seq, cmt in enumerate(series.commits):
            if seq != seq_to_drop:
                tree_id, cherry = self._pick_commit(repo, cmt)
                cur = self._finish_commit(repo, tree_id, cherry, cur)
        self._finish_process(repo, branch, name, quiet=True)

    def _process_series(self, name, series, new_name=None, dry_run=False):
        """Rewrite a series

        Args:
            name (str): Name of the branch to process
            series (Series): Series object
            new_name (str or None): New name, if a new branch is to be created
            dry_run (bool): True to do a dry run, restoring the original tree
                afterwards

        Return:
            pygit.oid: oid of the new branch
        """
        count = len(series.commits)
        repo, cur, branch, name = self._prepare_process(name, count, new_name)
        vals = SimpleNamespace()
        vals.final = False
        tout.info(f"Processing {count} commits from branch '{name}'")
        for seq, cmt in enumerate(series.commits):
            tree_id, cherry = self._pick_commit(repo, cmt)
            vals.cherry = cherry
            vals.msg = cherry.message
            vals.skip = False
            vals.info = ''
            vals.final = seq == len(series.commits) - 1
            vals.seq = seq
            yield vals

            cur = self._finish_commit(repo, tree_id, cherry, cur, vals.msg)
            tout.info(f"- {vals.info} {oid(cmt.hash)} as {oid(cur.target)}: {cmt}")
        target = self._finish_process(repo, branch, name, new_name, dry_run)
        vals.oid = target.oid

    def _mark_series(self, name, series, dry_run=False):
        """Mark a series with Change-Id tags

        Args:
            name (str): Name of the series to mark
            series (Series): Series object
            dry_run (bool): True to do a dry run, restoring the original tree
                afterwards

        Return:
            pygit.oid: oid of the new branch
        """
        vals = None
        for vals in self._process_series(name, series, dry_run=dry_run):
            if CHANGE_ID_TAG not in vals.msg:
                change_id = self.make_change_id(vals.cherry)
                vals.msg = vals.msg + f'\n{CHANGE_ID_TAG}: {change_id}'
                tout.detail("   - adding mark")
                vals.info = 'marked'
            else:
                vals.info = 'has mark'

        return vals.oid

    def mark_series(self, in_name, allow_marked=False, dry_run=False):
        """Add Change-Id tags to a series

        Args:
            in_name (str): Name of the series to unmark
            allow_marked (bool): Allow commits to be (already) marked
            dry_run (bool): True to do a dry run, restoring the original tree
                afterwards

        Return:
            pygit.oid: oid of the new branch
        """
        name, ser, _, _ = self._prep_series(in_name)
        tout.info(f"Marking series '{name}': allow_marked {allow_marked}")

        if not allow_marked:
            bad = []
            for cmt in ser.commits:
                if cmt.change_id:
                    bad.append(cmt)
            if bad:
                print(f'{len(bad)} commit(s) already have marks')
                for cmt in bad:
                    print(f' - {oid(cmt.hash)} {cmt.subject}')
                raise ValueError(
                    f'Marked commits {len(bad)}/{len(ser.commits)}')
        new_oid = self._mark_series(in_name, ser, dry_run=dry_run)

        if dry_run:
            tout.info('Dry run completed')
        return new_oid

    def unmark_series(self, name, allow_unmarked=False, dry_run=False):
        """Remove Change-Id tags from a series

        Args:
            name (str): Name of the series to unmark
            allow_unmarked (bool): Allow commits to be (already) unmarked
            dry_run (bool): True to do a dry run, restoring the original tree
                afterwards

        Return:
            pygit.oid: oid of the new branch
        """
        name, ser, _, _ = self._prep_series(name)
        tout.info(f"Unmarking series '{name}': allow_unmarked {allow_unmarked}")

        if not allow_unmarked:
            bad = []
            for cmt in ser.commits:
                if not cmt.change_id:
                    bad.append(cmt)
            if bad:
                print(f'{len(bad)} commit(s) are missing marks')
                for cmt in bad:
                    print(f' - {oid(cmt.hash)} {cmt.subject}')
                raise ValueError(
                    f'Unmarked commits {len(bad)}/{len(ser.commits)}')
        vals = None
        for vals in self._process_series(name, ser, dry_run=dry_run):
            if CHANGE_ID_TAG in vals.msg:
                lines = vals.msg.splitlines()
                updated = [line for line in lines
                           if not line.startswith(CHANGE_ID_TAG)]
                vals.msg = '\n'.join(updated)

                tout.detail("   - removing mark")
                vals.info = 'unmarked'
            else:
                vals.info = 'no mark'

        if dry_run:
            tout.info('Dry run completed')
        return vals.oid

    def add_upstream(self, name, url):
        """Add a new upstream tree

        Args:
            name (str): Name of the tree
            url (str): URL for the tree
        """
        try:
            self.db.execute(
                f"INSERT INTO upstream (name, url) VALUES ('{name}', '{url}')")
        except sqlite3.IntegrityError as exc:
            if 'UNIQUE constraint failed: upstream.name' in str(exc):
                raise ValueError(f"Upstream '{name}' already exists") from exc
        self.commit()

    def list_upstream(self):
        """List the upstream repos

        Shows a list of the repos, obtained from the database
        """
        udict = self.get_upstream_dict()

        for name, items in udict.items():
            url, is_default = items
            default = 'default' if is_default else ''
            print(f'{name:15.15} {default:8} {url}')

    def set_default_upstream(self, name):
        """Set the default upstream target

        Args:
            name (str): Name of the upstream remote to set as default, or None
                for none
        """
        self.db.execute("UPDATE upstream SET is_default = 0")
        if name is not None:
            self.db.execute(
                f"UPDATE upstream SET is_default = 1 WHERE name = '{name}'")
            if self.rowcount() != 1:
                self.rollback()
                raise ValueError(f"No such upstream '{name}'")
        self.commit()

    def get_default_upstream(self):
        """Get the default upstream target

        Return:
            str: Name of the upstream remote to set as default, or None if none
        """
        res = self.db.execute(
            "SELECT name FROM upstream WHERE is_default = 1")
        recs = res.fetchall()
        if len(recs) != 1:
            return None
        return recs[0][0]

    def delete_upstream(self, name):
        """Delete an upstream target

        Args:
            name (str): Name of the upstream remote to delete
        """
        self.db.execute(
            f"DELETE FROM upstream WHERE name = '{name}'")
        if self.rowcount() != 1:
            self.rollback()
            raise ValueError(f"No such upstream '{name}'")
        self.commit()

    def remove_series(self, name, dry_run=False):
        """Remove a series from the database

        Args:
            name (str): Name of series to remove, or None to use current one
            dry_run (bool): True to do a dry run
        """
        ser = self.parse_series(name)
        name = ser.name

        res = self.db.execute(
            f"DELETE FROM series WHERE name = '{name}'")
        if self.rowcount() != 1:
            self.rollback()
            raise ValueError(f"No such series '{name}'")

        res = self.db.execute('SELECT id FROM ser_ver WHERE series_id = ?',
                               (ser.idnum,))
        recs = [str(i) for i in res.fetchall()[0]]
        vals = ', '.join(recs[0])
        res = self.db.execute(f'DELETE FROM pcommit WHERE svid IN ({vals})')
        res = self.db.execute('DELETE FROM ser_ver WHERE series_id = ?',
                               (ser.idnum,))
        if not dry_run:
            self.commit()
        else:
            self.rollback()

        self.commit()
        tout.info(f"Removed series '{name}'")
        if dry_run:
            tout.info('Dry run completed')

    def remove_version(self, name, version, dry_run=False):
        """Remove a version of a series from the database

        Args:
            name (str): Name of series to remove, or None to use current one
            version (int): Version number to remove
            dry_run (bool): True to do a dry run
        """
        ser, version = self.parse_series_and_version(name, version)
        name = ser.name

        versions = self.ensure_version(ser, version)

        if versions == [version]:
            raise ValueError(
                f"Series '{ser.name}' only has one version: remove the series")

        svid = self.get_series_svid(ser.idnum, version)
        self.db.execute('DELETE FROM pcommit WHERE svid = ?', (svid,))
        self.db.execute(
            'DELETE FROM ser_ver WHERE series_id = ? and version = ?',
            (ser.idnum, version))
        if not dry_run:
            self.commit()
        else:
            self.rollback()

        tout.info(f"Removed version {version} from series '{name}'")
        if dry_run:
            tout.info('Dry run completed')

    def find_series_by_name(self, name):
        """Find a series and return its details

        Args:
            name (str): Name to search for

        Returns:
            idnum, or None if not found
        """
        res = self.db.execute(
            'SELECT id FROM series WHERE '
            f"name = '{name}' AND archived = 0")
        recs = res.fetchall()
        if len(recs) != 1:
            return None
        return recs[0][0]

    def set_project(self, pwork, name, quiet=False):
        """Set the name of the project

        Args:
            pwork (Patchwork): Patchwork object to use
            name (str): Name of the project to use in patchwork
            quiet (bool): True to skip writing the message
        """
        res = self.loop.run_until_complete(pwork.get_projects())
        proj_id = None
        for proj in res:
            if proj['name'] == name:
                proj_id = proj['id']
                link_name = proj['link_name']
        if not proj_id:
            raise ValueError(f"Unknown project name '{name}'")
        res = self.db.execute('DELETE FROM settings')
        res = self.db.execute(
                'INSERT INTO settings (name, proj_id, link_name) '
                'VALUES (?, ?, ?)',
                (name, proj_id, link_name))
        self.commit()
        if not quiet:
            tout.info(f"Project '{name}' patchwork-ID {proj_id} link-name {link_name}")

    def get_project(self):
        """Get the details of the project

        Returns:
            tuple:
                name (str): Project name, e.g. 'U-Boot'
                proj_id (int): Patchworks project ID for this project
                link_name (str): Patchwork's link-name for the project
        """
        res = self.db.execute("SELECT name, proj_id, link_name FROM settings")
        recs = res.fetchall()
        if len(recs) != 1:
            return None
        return recs[0]

    def build_col(self, state, prefix='', base_str=None):
        """Build a patch-state string with colour

        Args:
            state (str): State to colourise (also indicates the colour to use)
            prefix (str): Prefix string to also colourise
            base_str (str or None): String to show instead of state, or None to
                show state

        Return:
            str: String with ANSI colour characters
        """
        bright = True
        if state == 'accepted':
            col = self.col.GREEN
        elif state == 'awaiting-upstream':
            bright = False
            col = self.col.GREEN
        elif state in ['changes-requested']:
            col = self.col.CYAN
        elif state in ['rejected', 'deferred', 'not-applicable',
                        'superseded', 'handled-elsewhere']:
            col = self.col.RED
        elif not state:
            state = 'unknown'
            col = self.col.MAGENTA
        else:
            # under-review, rfc, needs-review-ack
            col = self.col.WHITE
        out = base_str or SHORTEN_STATE.get(state, state)
        pad = ' ' * (10 - len(out))
        col_state = self.col.build(col, prefix + out, bright)
        return col_state, pad

    def _list_patches(self, branch, pwc, series, desc, cover_id, num_comments,
                      show_commit, show_patch, list_patches, state_totals):
        """List patches along with optional status info

        Args:
            branch (str): Branch name        if self.show_progress
            pwc (dict): pcommit records:
                key (int): seq
                value (PCOMMIT): Record from database
            series (Series): Series to show, or None to just use the database
            desc (str): Series title
            cover_id (int): Cover-letter ID
            num_comments (int): The number of comments on the cover letter
            show_commit (bool): True to show the commit and diffstate
            show_patch (bool): True to show the patch
            list_patches (bool): True to list all patches for each series,
                False to just show the series summary on a single line
            state_totals (dict): Holds totals for each state across all patches
                key (str): state name
                value (int): Number of patches in that state
        """
        lines = []
        states = defaultdict(int)
        count = len(pwc)
        for seq, item in enumerate(pwc.values()):
            if series:
                cmt = series.commits[seq]
                if cmt.subject != item.subject:
                    tout.warning(f'''Inconsistent commit-subject:
Commit: {cmt.hash}
Database: '{item.subject}'
Branch:   '{cmt.subject}
Please use 'patman series -s {branch} scan' to resolve this''')

            col_state, pad = self.build_col(item.state)
            patch_id = item.patch_id if item.patch_id else ''
            if item.num_comments:
                comments = str(item.num_comments)
            elif item.num_comments is None:
                comments = '-'
            else:
                comments = ''

            if show_commit or show_patch:
                subject = self.col.build(self.col.BLACK, item.subject,
                                         bright=False, back=self.col.YELLOW)
            else:
                subject = item.subject

            line = (f'{seq:3} {col_state}{pad} {comments.rjust(3)} '
                    f'{patch_id:7} {oid(cmt.hash)} {subject}')
            lines.append(line)
            states[item.state] += 1
        out = ''
        for state, freq in states.items():
            out += ' ' + self.build_col(state, f'{freq}:')[0]
            state_totals[state] += freq
        name = ''
        if not list_patches:
            name = desc or ''
            name = self.col.build(self.col.YELLOW, name[:41].ljust(41))
            print(f"{branch:16} {name} {len(pwc):5} {out}")
            return
        print(f"Branch '{branch}' (total {len(pwc)}):{out}{name}")

        print(self.col.build(
            self.col.MAGENTA,
            f"Seq State      Com PatchId {'Commit'.ljust(HASH_LEN)} Subject"))

        comments = '' if num_comments is None else str(num_comments)
        if desc or comments or cover_id:
            cov = 'Cov' if cover_id else ''
            print(self.col.build(
                self.col.WHITE,
                f"{cov:14} {comments.rjust(3)} {cover_id or '':7}            {desc}",
                bright=False))
        for seq in range(count):
            line = lines[seq]
            print(line)
            if show_commit or show_patch:
                print()
                cmt = series.commits[seq] if series else ''
                msg = gitutil.show_commit(
                    cmt.hash, show_commit, True, show_patch,
                    colour=self.col.enabled(), git_dir=self.gitdir)
                sys.stdout.write(msg)
                if seq != count - 1:
                    print()
                    print()

    def _get_patches(self, series, version):
        """Get a Series object containing the patches in a series

        Args:
            series (str): Name of series to use, or None to use current branch
            version (int): Version number, or None to detect from name

        Return: tuple:
            str: Name of branch, e.g. 'mary2'
            Series: Series object containing the commits and idnum, desc, name
            int: Version number of series, e.g. 2
            OrderedDict:
                key (int): record ID if find_svid is None, else seq
                value (PCOMMIT): record data
            str: series name (for this version)
            str: cover_id
            int: cover_num_comments
        """
        ser, version = self.parse_series_and_version(series, version)
        if not ser.idnum:
            raise ValueError(f"Unknown series '{series}'")
        self.ensure_version(ser, version)
        svid, link, cover_id, num_comments, name = self.get_ser_ver(ser.idnum,
                                                                    version)
        pwc = self.get_pcommit_dict(svid)

        count = len(pwc)
        branch = self.join_name_version(ser.name, version)
        series = patchstream.get_metadata(branch, 0, count, git_dir=self.gitdir)
        self.copy_db_fields_to(series, ser)

        return branch, series, version, pwc, name, link, cover_id, num_comments

    def list_patches(self, series, version, show_commit=False,
                     show_patch=False):
        """List patches in a series

        Args:
            series (str): Name of series to use, or None to use current branch
            version (int): Version number, or None to detect from name
            show_commit (bool): True to show the commit and diffstate
            show_patch (bool): True to show the patch
        """
        branch, series, version, pwc, name, _, cover_id, num_comments = (
            self._get_patches(series, version))
        with terminal.pager():
            state_totals = defaultdict(int)
            self._list_patches(branch, pwc, series, name, cover_id,
                               num_comments, show_commit, show_patch, True,
                               state_totals)

    def get_series_svid(self, series_id, version):
        """Get the patchwork ID of a series version

        Args:
            series_id (int): id of the series to look up
            version (int): version number to look up

        Return:
            str: link found

        Raises:
            ValueError: No matching series found
        """
        return self.get_series_svid_link(series_id, version)[0]

    def get_series_svid_link(self, series_id, version):
        """Get the patchwork ID of a series version

        Args:
            series_id (int): series ID to look up
            version (int): version number to look up

        Return:
            tuple:
                int: record id
                str: link
        """
        recs = self.get_ser_ver(series_id, version)
        return recs[0:2]

    def get_ser_ver(self, series_id, version):
        """Get the patchwork ID of a series version

        Args:
            series_id (int): series ID to look up
            version (int): version number to look up

        Return: tuple:
            int: record id
            str: link
            str: cover_id
            int: cover_num_comments
            str: cover-letter name
        """
        res = self.db.execute(
            'SELECT id, link, cover_id, cover_num_comments, name FROM ser_ver '
            'WHERE series_id = ? AND version = ?', (series_id, version))
        recs = res.fetchall()
        if not recs:
            raise ValueError(
                f'No matching series for id {series_id} version {version}')
        return recs[0]

    def _sync_one(self, svid, cover, patches):
        """Sync one series to the database

        Args:
            svid (int): Ser/ver ID
            cover (dict or None): Cover letter from patchwork, with keys:
                id (int): Cover-letter ID in patchwork
                num_comments (int): Number of comments
                name (str): Cover-letter name
            patches (list of Patch): Patches in the series
        """
        pwc = self.get_pcommit_dict(svid)

        updated = 0
        for seq, item in enumerate(pwc.values()):
            if seq >= len(patches):
                continue
            patch = patches[seq]
            if patch.id:
                self.db.execute(
                    'UPDATE pcommit SET '
                    'patch_id = ?, state = ?, num_comments = ? WHERE id = ?',
                    (patch.id, patch.state, len(patch.comments), item.id))
                updated += self.rowcount()
        if cover:
            self.db.execute(
                'UPDATE ser_ver SET cover_id = ?, cover_num_comments = ?, '
                'name = ? WHERE id = ?',
                (cover.id, cover.num_comments, cover.name, svid))
        else:
            self.db.execute('UPDATE ser_ver SET name = ? WHERE id = ?',
                            (patches[0].name, svid))

        return updated

    async def _series_sync(self, client, pwork, name, version,
                           show_comments, show_cover_comments, gather_tags,
                           dry_run):
        """Sync the series status from patchwork

        Args:
            pwork (Patchwork): Patchwork object to use
            name (str): Name of series to use, or None to use current branch
            version (int): Version number, or None to detect from name
            show_comments (bool): True to show the comments on each patch
            show_cover_comments (bool): True to show the comments on the cover
                letter
            gather_tags (bool): True to gather review/test tags
            dry_run (bool): True to do a dry run
        """
        ser, version = self.parse_series_and_version(name, version)
        self.ensure_version(ser, version)
        svid, link = self.get_series_svid_link(ser.idnum, version)
        if not link:
            raise ValueError(
                "No patchwork link is available: use 'patman series autolink'")
        tout.info(
            f"Updating series '{ser.name}' version {version} from link '{link}'")
        cover, patches = await pwork._series_get_state(
            client, link, True, show_cover_comments)
        if gather_tags:
            pwc = self.get_pcommit_dict(svid)
            count = len(pwc)
            branch = self.join_name_version(ser.name, version)
            series = patchstream.get_metadata(branch, 0, count,
                                              git_dir=self.gitdir)

            _, new_rtag_list, cover, patches = status.show_status(
                cover, patches,
                series, link, self.get_branch_name(ser.name, version),
                show_comments, show_cover_comments)
            self.update_series(ser.name, series, version, None, dry_run,
                               add_rtags=new_rtag_list)

        updated = self._sync_one(svid, cover, patches)
        tout.info(f"{updated} patch{'es' if updated != 1 else ''}"
                  f"{' and cover letter' if cover else ''} updated")

        if not dry_run:
            self.commit()
        else:
            self.rollback()
            tout.info('Dry run completed')

    async def do_series_sync(self, pwork, series, version, show_comments,
                             show_cover_comments, gather_tags, dry_run):
        async with aiohttp.ClientSession() as client:
            await self._series_sync(client, pwork, series, version,
                                    show_comments, show_cover_comments,
                                    gather_tags, dry_run)

    def series_sync(self, pwork, series, version, show_comments,
                    show_cover_comments, gather_tags, dry_run=False):
        loop = asyncio.get_event_loop()
        loop.run_until_complete(self.do_series_sync(
            pwork, series, version, show_comments, show_cover_comments,
            gather_tags, dry_run))

    def _get_fetch_dict(self, sync_all_versions):
        """Get a dict of ser_vers to fetch, along with their patchwork links

        Args:
            sync_all_versions (bool): True to sync all versions of a series,
                False to sync only the latest version

        Return: tuple:
            dict: things to fetch
                key (int): svid
                value (str): patchwork link for the series
            int: number of series which are missing a link
        """
        missing = 0
        sdict = self.get_ser_ver_dict()
        to_fetch = {}

        if sync_all_versions:
            for svid, series_id, version, link, _, _, desc in \
                    self.get_ser_ver_list():
                name, _ = self.get_series_info(series_id)
                pwc = self.get_pcommit_dict(svid)
                count = len(pwc)
                branch = self.join_name_version(name, version)
                series = patchstream.get_metadata(branch, 0, count,
                                                  git_dir=self.gitdir)
                if link:
                    to_fetch[svid] = patchwork.STATE_REQ(
                        link, desc, series_id, series, branch, False, False)
                else:
                    missing += 1
        else:
            # Find the maximum version for each series
            max_vers = self.series_all_max_versions()

            # Get a list of links to fetch
            for svid, series_id, version in max_vers:
                ser = sdict[svid]
                name, _ = self.get_series_info(series_id)
                pwc = self.get_pcommit_dict(svid)
                count = len(pwc)
                branch = self.join_name_version(name, version)
                series = patchstream.get_metadata(branch, 0, count,
                                                  git_dir=self.gitdir)
                if ser.link:
                    to_fetch[svid] = patchwork.STATE_REQ(
                        ser.link, ser.name, series_id, series, branch, False,
                        False)
                else:
                    missing += 1
        return to_fetch, missing

    def series_sync_all(self, pwork, sync_all_versions=False,
                        gather_tags=False, dry_run=False):
        """Sync all series status from patchwork

        Args:
            pwork (Patchwork): Patchwork object to use
            sync_all_versions (bool): True to sync all versions of a series,
                False to sync only the latest version
            gather_tags (bool): True to gather review/test tags
        """
        to_fetch, missing = self._get_fetch_dict(sync_all_versions)

        result, requests = self.loop.run_until_complete(
                pwork.series_get_states(to_fetch, gather_tags))

        updated = 0
        updated_cover = 0
        for resp in result:
            updated += self._sync_one(resp.svid, resp.cover, resp.patches)
            if resp.cover:
                updated_cover += 1
        self.commit()

        tout.info(
            f"{updated} patch{'es' if updated != 1 else ''} and "
            f"{updated_cover} cover letter{'s' if updated_cover != 1 else ''} "
            f'updated, {missing} missing '
            f"link{'s' if missing != 1 else ''} ({requests} requests)")
        if not dry_run:
            self.commit()
        else:
            self.rollback()
            tout.info('Dry run completed')

    def series_max_version(self, idnum):
        """Find the latest version of a series

        Args:
            idnum (int): Series ID to look up

        Return:
            int: maximum version
        """
        res = self.db.execute('SELECT MAX(version) FROM ser_ver WHERE '
                               f"series_id = {idnum}")
        return res.fetchall()[0][0]

    def series_all_max_versions(self):
        """Find the latest version of all series

        Return: list of:
            int: ser_ver ID
            int: series ID
            int: Maximum version
        """
        res = self.db.execute('SELECT id, series_id, MAX(version) FROM ser_ver '
                              'GROUP BY series_id')
        versions = res.fetchall()
        return versions

    def _progress_one(self, ser, show_all_versions, list_patches, state_totals):
        """Show progress information for all versions in a series

        Args:
            ser (Series): Series to use
            show_all_versions (bool): True to show all versions of a series,
                False to show only the final version
            list_patches (bool): True to list all patches for each series,
                False to just show the series summary on a single line
            state_totals (dict): Holds totals for each state across all patches
                key (str): state name
                value (int): Number of patches in that state

        Return: tuple
            int: Number of series shown
            int: Number of patches shown
        """
        max_vers = self.series_max_version(ser.idnum)
        name, desc = self.get_series_info(ser.idnum)
        coloured = self.col.build(self.col.BLACK, desc, bright=False,
                                  back=self.col.YELLOW)
        versions = self.get_version_list(ser.idnum)
        vstr = list(map(str, versions))

        if list_patches:
            print(f"{name}: {coloured} (versions: {' '.join(vstr)})")
        add_blank_line = False
        total_series = 0
        total_patches = 0
        for ver in versions:
            if not show_all_versions and ver != max_vers:
                continue
            if add_blank_line:
                print()
            _, pwc = self.series_get_version_stats(ser.idnum, ver)
            count = len(pwc)
            branch = self.join_name_version(ser.name, ver)
            series = patchstream.get_metadata(branch, 0, count,
                                              git_dir=self.gitdir)
            _, _, cover_id, num_comments, name = self.get_ser_ver(ser.idnum,
                                                                  ver)

            self._list_patches(branch, pwc, series, name, cover_id,
                               num_comments, False, False, list_patches,
                               state_totals)
            add_blank_line = list_patches
            total_series += 1
            total_patches += count
        return total_series, total_patches

    def progress(self, series, show_all_versions, list_patches):
        """Show progress information for all versions in a series

        Args:
            series (str): Name of series to use, or None to show progress for
                all series
            show_all (bool): True to show all versions of a series, False to
                show only the final version
        """
        with terminal.pager():
            state_totals = defaultdict(int)
            if series is not None:
                self._progress_one(self.parse_series(series), show_all_versions,
                                   list_patches, state_totals)
                return

            total_patches = 0
            total_series = 0
            sdict = self.get_series_dict()
            border = None
            if not list_patches:
                print(self.col.build(
                    self.col.MAGENTA,
                    f"{'Name':16} {'Description':41} Count  {'Status'}"))
                border = f"{'-' * 15}  {'-' * 40}  -----  {'-' * 15}"
                print(border)
            for name in sorted(sdict):
                ser = sdict[name]
                num_series, num_patches = self._progress_one(
                    ser, show_all_versions, list_patches, state_totals)
                if list_patches:
                    print()
                total_series += num_series
                total_patches += num_patches
            if not list_patches:
                print(border)
                total = f'{total_series} series'
                out = ''
                for state, freq in state_totals.items():
                    out += ' ' + self.build_col(state, f'{freq}:')[0]

                print(f"{total:15}  {'':40}  {total_patches:5} {out}")

    def _summary_one(self, ser):
        """Show summary information for the latest version in a series

        Args:
            series (str): Name of series to use, or None to show progress for
                all series
        """
        max_vers = self.series_max_version(ser.idnum)
        name, desc = self.get_series_info(ser.idnum)
        stats, pwc = self.series_get_version_stats(ser.idnum, max_vers)
        states = {x.state for x in pwc.values()}
        state = 'accepted'
        for val in ['awaiting-upstream', 'changes-requested', 'rejected',
                    'deferred', 'not-applicable', 'superseded',
                    'handled-elsewhere']:
            if val in states:
                state = val
        state_str, pad = self.build_col(state, base_str=name)
        print(f"{state_str}{pad}  {stats.rjust(6)}  {desc}")

    def summary(self, series):
        """Show summary information for all series

        Args:
            series (str): Name of series to use
        """
        print(f"{'Name':17}  Status  Description")
        print(f"{'-' * 17}  {'-' * 6}  {'-' * 30}")
        if series is not None:
            self._summary_one(self.parse_series(series))
            return

        sdict = self.get_series_dict()
        for ser in sdict.values():
            self._summary_one(ser)

    def open_series(self, pwork, name, version):
        """Open the patchwork page for a series

        Args:
            pwork (Patchwork): Patchwork object to use
            name (str): Name of series to open
            version (str): Version number to open
        """
        ser, version = self.parse_series_and_version(name, version)
        link = self.get_link(ser.name, version)
        pwork.url = 'https://patchwork.ozlabs.org'
        url = self.loop.run_until_complete(pwork.get_series_url(link))
        print(f'Opening {url}')

        # With Firefox, GTK produces lots of warnings, so suppress them
        # Gtk-Message: 06:48:20.692: Failed to load module "xapp-gtk3-module"
        # Gtk-Message: 06:48:20.692: Not loading module "atk-bridge": The
        #  functionality is provided by GTK natively. Please try to not load it.
        # Gtk-Message: 06:48:20.692: Failed to load module "appmenu-gtk-module"
        # Gtk-Message: 06:48:20.692: Failed to load module "appmenu-gtk-module"
        # [262145, Main Thread] WARNING: GTK+ module /snap/firefox/5987/gnome-platform
        #  /usr/lib/gtk-2.0/modules/libcanberra-gtk-module.so cannot be loaded.
        # GTK+ 2.x symbols detected. Using GTK+ 2.x and GTK+ 3 in the same process
        #   is not supported.: 'glib warning', file /build/firefox/parts/firefox/
        #   build/toolkit/xre/nsSigHandlers.cpp:201
        #
        # (firefox_firefox:262145): Gtk-WARNING **: 06:48:20.728: GTK+ module
        #   /snap/firefox/5987/gnome-platform/usr/lib/gtk-2.0/modules/libcanberra-gtk-module.so
        #   cannot be loaded.
        # GTK+ 2.x symbols detected. Using GTK+ 2.x and GTK+ 3 in the same process is not supported.
        # Gtk-Message: 06:48:20.728: Failed to load module "canberra-gtk-module"
        # [262145, Main Thread] WARNING: GTK+ module /snap/firefox/5987/gnome-platform/
        #   usr/lib/gtk-2.0/modules/libcanberra-gtk-module.so cannot be loaded.
        # GTK+ 2.x symbols detected. Using GTK+ 2.x and GTK+ 3 in the same process is not
        #   supported.: 'glib warning', file /build/firefox/parts/firefox/build/
        #   toolkit/xre/nsSigHandlers.cpp:201
        #
        # (firefox_firefox:262145): Gtk-WARNING **: 06:48:20.729: GTK+ module
        #   /snap/firefox/5987/gnome-platform/usr/lib/gtk-2.0/modules/
        #   libcanberra-gtk-module.so cannot be loaded.
        # GTK+ 2.x symbols detected. Using GTK+ 2.x and GTK+ 3 in the same process is not supported.
        # Gtk-Message: 06:48:20.729: Failed to load module "canberra-gtk-module"
        # ATTENTION: default value of option mesa_glthread overridden by environment.
        cros_subprocess.Popen(['xdg-open', url])

    def _find_matched_commit(self, commits, pcm):
        """Find a commit in a list of possible matches

        Args:
            commits (dict of Commit): Possible matches
                key (int): sequence number of patch (from 0)
                value (Commit): Commit object
            pcm (PCOMMIT): Patch to check

        Return:
            int: Sequence number of matching commit, or None if not found
        """
        for seq, cmt in commits.items():
            tout.debug(f"- match subject: '{cmt.subject}'")
            if pcm.subject == cmt.subject:
                return seq
        return None

    def _find_matched_patch(self, patches, cmt):
        """Find a patch in a list of possible matches

        Args:
            patches: dict of ossible matches
                key (int): sequence number of patch
                value (PCOMMIT): patch
            cmt (Commit): Commit to check

        Return:
            int: Sequence number of matching patch, or None if not found
        """
        for seq, pcm in patches.items():
            tout.debug(f"- match subject: '{pcm.subject}'")
            if cmt.subject == pcm.subject:
                return seq
        return None

    def scan(self, branch_name, mark=False, allow_unmarked=False, end=None,
             dry_run=False):
        """Scan a branch and make updates to the database if it has changed

        Args:
            branch_name (str): Name of branch to sync, or None for current one
            mark (str): True to mark each commit with a change ID
            allow_unmarked (str): True to not require each commit to be marked
            end (str): Add only commits up to but exclu
            dry_run (bool): True to do a dry run
        """
        def _show_item(oper, seq, subject):
            col = None
            if oper == '+':
                col = self.col.GREEN
            elif oper == '-':
                col = self.col.RED
            out = self.col.build(col, subject) if col else subject
            tout.info(f'{oper} {seq:3} {out}')

        name, ser, version, msg = self._prep_series(branch_name, end)
        svid = self.get_ser_ver(ser.idnum, version)[0]
        pcdict = self.get_pcommit_dict(svid)

        tout.info(
            f"Syncing series '{name}' v{version}: mark {mark} allow_unmarked {allow_unmarked}")
        if msg:
            tout.info(msg)

        ser = self._handle_mark(name, ser, version, mark, allow_unmarked,
                                False, dry_run)

        # First check for new patches that are not in the database
        to_add = dict(enumerate(ser.commits))
        for pcm in pcdict.values():
            tout.debug(f'pcm {pcm.subject}')
            i = self._find_matched_commit(to_add, pcm)
            if i is not None:
                del to_add[i]

        # Now check for patches in the database that are not in the branch
        to_remove = dict(enumerate(pcdict.values()))
        for cmt in ser.commits:
            tout.debug(f'cmt {cmt.subject}')
            i = self._find_matched_patch(to_remove, cmt)
            if i is not None:
                del to_remove[i]

        for seq, cmt in enumerate(ser.commits):
            if seq in to_remove:
                _show_item('-', seq, to_remove[seq].subject)
                del to_remove[seq]
            if seq in to_add:
                _show_item('+', seq, to_add[seq].subject)
                del to_add[seq]
            else:
                _show_item(' ', seq, cmt.subject)
        seq = len(ser.commits)
        for cmt in to_add.items():
            _show_item('+', seq, cmt.subject)
            seq += 1
        for seq, pcm in to_remove.items():
            _show_item('+', seq, pcm.subject)

        self.db.execute('DELETE FROM pcommit WHERE svid = ?', (svid,))
        self.add_series_commits(ser, svid)
        if not dry_run:
            self.commit()
        else:
            self.rollback()
            tout.info('Dry run completed')

    def send_series(self, pwork, name, autolink, autolink_wait, args):
        """Send out a series

        Args:
            pwork (Patchwork): Patchwork object to use
            series (str): Series name to search for, or None for current series
                that is checked out
            autolink (bool): True to auto-link the series after sending
            args: 'send' arguments provided
            autolink_wait (int): Number of seconds to wait for the autolink to
                succeed
        """
        ser, version = self.parse_series_and_version(name, None)
        if not name:
            name = self.get_branch_name(ser.name, version)
        if not ser.idnum:
            raise ValueError(f"Series '{ser.name}' not found in database")

        send.send(args, git_dir=self.gitdir, cwd=self.topdir)

        if not args.dry_run and autolink:
            self.autolink(pwork, name, version, True, wait_s=autolink_wait)

    def series_status(self, pwork, series, version, show_comments,
                      show_cover_comments=False, single_thread=False):
        """Show the series status from patchwork

        Args:
            pwork (Patchwork): Patchwork object to use
            series (str): Name of series to use, or None to use current branch
            version (int): Version number, or None to detect from name
            show_comments (bool): Show all comments on each patch
            show_cover_comments (bool): Show all comments on the cover letter
            single_thread (bool): Avoid using the threads
        """
        branch, series, version, _, _, link, _, _ = self._get_patches(
            series, version)
        if not link:
            raise ValueError(
                f"Series '{series.name}' v{version} has no patchwork link: "
                f"Try 'patman series -s {branch} autolink'")
        status.check_and_report_status(
            series, link, branch, None, False, show_comments,
            show_cover_comments, pwork, self.gitdir, single_thread)

    def series_rename(self, series, name, dry_run=False):
        """Rename a series

        Renames a series and changes the name of any branches which match
        versions present in the database

        Args:
            series (str): Name of series to use, or None to use current branch
            name (str): new name to use (must not include version number)
            dry_run (bool): True to do a dry run
        """
        old_ser, _ =  self.parse_series_and_version(series, None)
        if not old_ser.idnum:
            raise ValueError(f"Series '{old_ser.name}' not found in database")
        if old_ser.name != series:
            raise ValueError(
                f"Invalid series name '{series}': did you use the branch name?")
        chk, _ = split_name_version(name)
        if chk != name:
            raise ValueError(
                f"Invalid series name '{name}': did you use the branch name?")
        if chk == old_ser.name:
            raise ValueError(f"Cannot rename series '{old_ser.name}' to itself")
        if self.get_series_by_name(name):
            raise ValueError(f"Cannot rename: series '{name}' already exists")

        versions = self.get_version_list(old_ser.idnum)
        missing = []
        exists = []
        todo = {}
        for ver in versions:
            ok = True
            old_branch = self.get_branch_name(old_ser.name, ver)
            if not gitutil.check_branch(old_branch, self.gitdir):
                missing.append(old_branch)
                ok = False

            branch = self.get_branch_name(name, ver)
            if gitutil.check_branch(branch, self.gitdir):
                exists.append(branch)
                ok = False

            if ok:
                todo[ver] = [old_branch, branch]

        if missing or exists:
            msg = 'Cannot rename'
            if missing:
                msg += f": branches missing: {', '.join(missing)}"
            if exists:
                msg += f": branches exist: {', '.join(exists)}"
            raise ValueError(msg)

        for old_branch, branch in todo.values():
            tout.info(f"Renaming branch '{old_branch}' to '{branch}'")
            if not dry_run:
                gitutil.rename_branch(old_branch, branch, self.gitdir)

        # Change the series name; nothing needs to change in ser_ver
        self.db.execute('UPDATE series SET name = ? WHERE id = ?',
                        (name, old_ser.idnum))

        if not dry_run:
            self.commit()
        else:
            self.rollback()

        tout.info(f"Renamed series '{series}' to '{name}'")
        if dry_run:
            tout.info('Dry run completed')
