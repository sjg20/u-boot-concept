# SPDX-License-Identifier: GPL-2.0+
#
# Copyright 2025 Google LLC
#
"""Handles the 'series' subcommand
"""

from collections import OrderedDict
import hashlib
import os
import re
import sqlite3
from sqlite3 import OperationalError
from types import SimpleNamespace

import pygit2

from patman import patchstream
from patman.series import Series
from u_boot_pylib import gitutil
from u_boot_pylib import tout


# Tag to use for Change IDs
CHANGE_ID_TAG = 'Change-Id'

# Length of hash to display
HASH_LEN = 10

def oid(oid_val):
    return str(oid_val)[:HASH_LEN];


class Cseries:
    """Database with information about series

    This class handles database read/write as well as operations in a git
    directory to update series information.
    """
    def __init__(self, topdir=None):
        self.topdir = topdir
        self.gitdir = None
        self.con = None
        self.cur = None
        self.quiet = False

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
                'name UNIQUE, desc, archived BIT)')

            # version (int): Version number of the link
            self.cur.execute(
                'CREATE TABLE patchwork (id INTEGER PRIMARY KEY AUTOINCREMENT,'
                'version INTEGER, link, series_id INTEGER,'
                'FOREIGN KEY (series_id) REFERENCES series (id))')

            self.cur.execute(
                'CREATE TABLE upstream (name UNIQUE, url, is_default BIT)')

            # cid is the Change-Id
            self.cur.execute(
                'CREATE TABLE pcommit (id INTEGER PRIMARY KEY AUTOINCREMENT,'
                'pwid INTEGER, subject, cid,'
                'FOREIGN KEY (pwid) REFERENCES patchwork (id))')

            self.cur.execute(
                'CREATE TABLE settings (name UNIQUE, pwid INT)')
            self.con.commit()
        return self.cur

    def open_database(self):
        if not self.topdir:
            self.topdir = gitutil.get_top_level()
            if not self.topdir:
                raise ValueError('No git repo detected in current directory')
        self.gitdir = os.path.join(self.topdir, '.git')
        fname = f'{self.topdir}/.patman.db'
        if not os.path.exists(fname):
            tout.warning(f'Creating new database {fname}')
        self.con = sqlite3.connect(fname, autocommit=False)

        self.cur = self.check_database(self.con)
        return self.cur

    def close_database(self):
        self.con.close()
        self.cur = None

    def get_series_dict(self, include_archived=False):
        """Get a dict of Series objects from the database

        Return:
            OrderedDict:
                key: series name
                value: Series with name and desc filled out
        """
        res = self.cur.execute('SELECT id, name, desc FROM series ' +
            ('WHERE archived = 0' if not include_archived else ''))
        sdict = OrderedDict()
        for idnum, name, desc in res.fetchall():
            ser = Series()
            ser.idnum = idnum
            ser.name = name
            ser.desc = desc
            sdict[name] = ser
        return sdict

    def get_patchwork_dict(self):
        """Get a list of patchwork entries from the database

        Return:
            value: list of tuple:
                int: series_id
                int: version
                link: link string, or ''
        """
        res = self.cur.execute('SELECT series_id, version, link FROM patchwork')
        return res.fetchall()

    def get_upstream_dict(self):
        """Get a list of upstream entries from the database

        Return:
            OrderedDict:
                key (str): upstream name
                value (str): url
        """
        res = self.cur.execute('SELECT name, url, is_default FROM upstream')
        udict = OrderedDict()
        for name, url, is_default in res.fetchall():
            udict[name] = url, is_default
        return udict

    def get_pcommit_dict(self):
        """Get a dict of all pcommits entries from the database

        Return:
            OrderedDict:
                key (int): record ID
                value (tuple): record data
                    idnum (int): record ID
                    subject (str): patch subject
                    pwid (int): link to patchwork series/version record
                    cid (str): Change-ID value
        """
        res = self.cur.execute('SELECT id, subject, pwid, cid FROM pcommit')
        pcdict = OrderedDict()
        for idnum, subject, pwid, cid in res.fetchall():
            pcdict[idnum] = idnum, subject, pwid, cid
        return pcdict

    def get_pcommit_dict_for_pwid(self, pwid):
        """Get a dict of selected pcommits entries from the database

        Finds the records associated with a particular series and version

        Args:
            pwid (int): patchwork ID of series version

        Return:
            OrderedDict:
                key (int): record ID
                value (tuple): record data
                    idnum (int): record ID
                    subject (str): patch subject
                    pwid (int): link to patchwork series/version record
                    cid (str): Change-ID value
        """
        res = self.cur.execute(
            'SELECT id, subject, pwid, cid FROM pcommit WHERE pwid = ?',
            (pwid,))
        pcdict = OrderedDict()
        for idnum, subject, pwid, cid in res.fetchall():
            pcdict[idnum] = idnum, subject, pwid, cid
        return pcdict

    def _prep_series(self, name):
        ser, version = self.parse_series_and_version(name, None)
        name = ser.name

        # First check we have a branch with this name
        if not gitutil.check_branch(name, git_dir=self.gitdir):
            raise ValueError(f"No branch named '{name}'")

        count = gitutil.count_commits_to_branch(name, self.gitdir)
        if not count:
            raise ValueError('Cannot detect branch automatically')

        series = patchstream.get_metadata(name, 0, count, git_dir=self.gitdir)
        return name, series, version

    def add_series(self, name, desc=None, mark=False, allow_unmarked=False,
                   dry_run=False):
        """Add a series to the database

        Args:
            in_name (str): Name of series to add, or None to use current one
            desc (str): Description to use, or None to use the series subject
            mark (str): True to mark each commit with a change ID
            allow_unmarked (str): True to not require each commit to be marked
            dry_run (bool): True to do a dry run

        Return:
            Series: Series information
        """
        name, series, version = self._prep_series(name)
        tout.info(f"Adding series '{name}': mark {mark} allow_unmarked {allow_unmarked}")
        if desc is None:
            if not series.cover:
                raise ValueError(
                    f"Branch '{name}' has no cover letter - please provide description")
            desc = series.cover[0]

        if mark:
            oid = self.mark_series(name, series, dry_run=dry_run)

            # Collect the commits again, as the hashes have changed
            series = patchstream.get_metadata(oid, 0, len(series.commits),
                                              git_dir=self.gitdir)

        bad_count = 0
        for commit in series.commits:
            if not commit.change_id:
                bad_count += 1
        if bad_count and not allow_unmarked:
            raise ValueError(
                f'{bad_count} commit(s) are unmarked; please use -m or -M')

        if 'version' in series and int(series.version) != version:
            raise ValueError(f"Series name '{name}' suggests version {version} "
                             f"but Series-version tag indicates {series.version}")

        link = series.get_link_for_version(version)

        msg = 'Added'
        added = False
        idnum = self.find_series_by_name(name)
        if not idnum:
            res = self.cur.execute(
                'INSERT INTO series (name, desc, archived) '
                f"VALUES ('{name}', '{desc}', 0)")
            idnum = self.cur.lastrowid
            added = True
            msg += f" series '{name}'"

        if version not in self.get_version_list(idnum):
            res = self.cur.execute(
                'INSERT INTO patchwork (version, link, series_id) VALUES'
                f"('{version}', ?, {idnum})", (link,))
            pwid = self.cur.lastrowid
            msg += f" version {version}"
            if not added:
                msg += f" to existing series '{name}'"
            added = True

            self.add_series_commits(series, pwid)
        if not added:
            tout.info(f"Series '{name}' version {version} already exists")
            msg = None
        elif not dry_run:
            self.con.commit()
        else:
            self.con.rollback()
            idnum = None
        ser = Series()
        ser.name = name
        ser.desc = desc
        ser.idnum = idnum

        if msg:
            tout.info(msg)
        if dry_run:
            tout.info('Dry run completed')
        return ser

    def add_series_commits(self, series, pwid):
        """Add a commits from a series into the database

        Args:
            series (Series): Series containing commits to add
            pwid (int): patchwork-table ID to use for each commit
        """
        for commit in series.commits:
            res = self.cur.execute(
                'INSERT INTO pcommit (pwid, subject, cid) '
                'VALUES (?, ?, ?)',
                (str(pwid), commit.subject, commit.change_id))

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
        ser.idnum, ser.name, ser.desc = desc = all[0]
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

    def set_link(self, series, version, link, update_commit):
        """Add / update a series-link link for a series

        Args:
            series (str): Name of series to use, or None to use current branch
            version (int): Version number, or None to detect from name
            link (str): Patchwork link-string for the series
            update_commit (bool): True to update the current commit with the
                link
        """
        ser, version = self.parse_series_and_version(series, version)
        self.ensure_version(ser, version)

        if update_commit:
            cur_name = gitutil.get_branch(self.gitdir)

            repo = pygit2.init_repository(self.gitdir)
            commit = repo.head.peel(pygit2.GIT_OBJ_COMMIT)
            new_msg = commit.message + f'\nSeries-links: {link}'
            amended = repo.amend_commit(commit, 'HEAD', message=new_msg)

            branch_name = self.get_branch_name(ser.name, version)
            branch = repo.lookup_branch(branch_name)

            if cur_name == branch_name:
                repo.head.set_target(amended)

        if link is None:
            link = ''
        tout.info(
            f"Setting link for series '{ser.name}' version {version} to {link}")
        res = self.cur.execute(
            f"UPDATE patchwork SET link = '{link}' WHERE "
            'series_id = ? AND version = ?', (ser.idnum, version))
        self.con.commit()

    def get_link(self, series, version):
        """Get the patchwork link for a version of a series

        Args:
            series (str): Name of series to use, or None to use current branch
            version (int): Version number

        Return:
            str: Patchwork link as a string, e.g. '12325'
        """
        ser = self.parse_series(series)
        self.ensure_version(ser, version)

        res = self.cur.execute('SELECT link FROM patchwork WHERE '
            f"series_id = {ser.idnum} AND version = '{version}'")
        all = res.fetchall()
        if not all:
            return None
        if len(all) > 1:
            raise ValueError('Expected one match, but multiple matches found')
        return all[0][0]

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
        ser, version = self.parse_series_and_version(series, version)
        self.ensure_version(ser, version)
        if not ser.desc:
            raise ValueError(f"Series '{ser.name}' has an empty description")

        pws, options = pwork.find_series(ser.desc, version)
        return pws, options, ser.name, version, ser.desc

    def do_auto_link(self, pwork, series, version, update_commit):
        """Automatically find a series link by looking in patchwork

        Args:
            pwork (Patchwork): Patchwork object to use
            series (str): Series name to search for, or None for current series
                that is checked out
            version (int): Version to search for, or None for current version
                detected from branch name
            update_commit (bool): True to update the current commit with the
                link
        """
        pws, options, name, version, desc = self.search_link(pwork, series,
                                                             version)

        if not pws:
            print(f"Possible matches for '{name}' version {version} desc '{desc}':")
            for opt in options:
                print('  Link  Version  Description')
                print(f"{opt['id']:5}  {opt['version']:7}  {opt['name']}")
            raise ValueError(f"Cannot find series '{desc}'")

        self.set_link(name, version, pws, update_commit)

    def get_version_list(self, idnum):
        """Get a list of the versions available for a series

        Args:
            idnum (int): ID of series to look up

        Return:
            str: List of versions
        """
        if idnum is None:
            raise ValueError('Unknown series idnum')
        res = self.cur.execute('SELECT version FROM patchwork WHERE '
            f"series_id = {idnum}")
        all = res.fetchall()
        return [item[0] for item in all]

    def split_name_version(self, in_name):
        m_ver = re.match(r'([^0-9]*)(\d*)', in_name)
        if m_ver:
            name = m_ver.group(1)
            if m_ver.group(2):
                version = int(m_ver.group(2))
            else:
                version = 1
        else:
            name = in_name
            version = 1
        return name, version

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
        name, version = self.split_name_version(name)
        ser = self.get_series_by_name(name)
        if not ser:
            ser = Series()
            ser.name = name
        return ser

    def parse_series_and_version(self, name, in_version):
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
        if not name:
            name = gitutil.get_branch(self.gitdir)
        name, version = self.split_name_version(name)
        if version > 99:
            raise ValueError(f"Version '{version}' exceeds 99")
        if not name:
            raise ValueError(f"Series name '{name}' cannot be a number")
        ser = self.get_series_by_name(name)
        if not ser:
            ser = Series()
            ser.name = name
        if in_version:
            version = in_version
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
        res = self.cur.execute(
            f'UPDATE series SET archived = {int(archived)} WHERE '
            f'id = {ser.idnum}')
        self.con.commit()

    def do_list(self):
        sdict = self.get_series_dict()

        print(f"{'Name':15} {'Description':20} Versions")
        for name, ser in sdict.items():
            versions = self.get_version_list(ser.idnum)
            vlist = ' '.join([str(ver) for ver in sorted(versions)])
            print(f'{name:15.15} {ser.desc:20.20} {vlist}')

    def increment(self, series_name, dry_run=False):
        """Increment a series to the next version and create a new branch

        Args:
            series (str): Name of series to use, or None to use current branch
            dry_run (bool): True to do a dry run
        """
        ser = self.parse_series(series_name)
        if not ser.idnum:
            raise ValueError(f"Series '{ser.name}' not found in database")

        # Find the current version
        res = self.cur.execute('SELECT MAX(version) FROM patchwork WHERE '
            f"series_id = {ser.idnum}")
        max_vers = res.fetchall()[0][0]

        # Create a new branch
        repo = pygit2.init_repository(self.gitdir)

        commit = repo.head.peel(pygit2.GIT_OBJ_COMMIT)
        lines = commit.message.splitlines()

        index = None
        vers = None
        for seq, line in enumerate(lines):
            mat = re.match('Series-version:(.*)', line)
            if mat:
                index = seq
                vers = int(mat.group(1))
        if not index:
            print('No existing Series-version found, using version 1')
            vers = 1
        if vers != max_vers:
            raise ValueError(
                f'Expected version {max_vers} but Series-version is {vers}')
        vers += 1

        new_name = f'{ser.name}{vers}'
        new_branch = repo.branches.create(new_name, commit)
        if not dry_run:
            repo.checkout(new_branch)

        ver_string = f'Series-version: {vers}'
        if index:
            lines = (lines[:index] + [ver_string] + lines[index + 1:])
        else:
            lines.append(ver_string)
        new_msg = '\n'.join(lines) + '\n'
        amended = repo.amend_commit(commit, 'HEAD', message=new_msg)

        old_pwid = self.get_series_pwid(ser.idnum, max_vers)
        pcd = self.get_pcommit_dict_for_pwid(old_pwid)

        res = self.cur.execute(
            'INSERT INTO patchwork (version, series_id) VALUES'
            f"('{vers}', {ser.idnum})")
        pwid = self.cur.lastrowid

        for idnum, subject, _, cid in pcd.values():
            res = self.cur.execute(
                'INSERT INTO pcommit (pwid, subject, cid) VALUES (?, ?, ?)',
                (str(pwid), subject, cid))

        if not dry_run:
            self.con.commit()
        else:
            self.con.rollback()


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

        # Find the current version
        res = self.cur.execute('SELECT MAX(version) FROM patchwork WHERE '
            f"series_id = {ser.idnum}")
        max_vers = res.fetchall()[0][0]
        if max_vers < 2:
            raise ValueError(f"Series '{ser.name}' only has one version")

        tout.info(f"Removing series '{ser.name}' version {max_vers}")

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

        old_pwid = self.get_series_pwid(ser.idnum, max_vers)

        res = self.cur.execute(
            'DELETE FROM patchwork WHERE series_id = ? and version = ?',
            (ser.idnum, max_vers))
        res = self.cur.execute(
            'DELETE FROM pcommit WHERE pwid = ?', (old_pwid,))
        if not dry_run:
            self.con.commit()
        else:
            self.con.rollback()

    def make_cid(self, commit):
        """Make a Change ID for a commit"""
        sig = commit.committer
        val = hashlib.sha1()
        to_hash = f'{sig.name} <{sig.email}> {sig.time} {sig.offset}'
        val.update(to_hash.encode('utf-8'))
        val.update(str(commit.tree_id).encode('utf-8'))
        val.update(commit.message.encode('utf-8'))
        return val.hexdigest()

        # commit.committer
        # git var GIT_COMMITTER_IDENT ; echo "$refhash" ; cat "README"; } | git hash-object --stdin)

    def _process_series(self, vals, name, series, dry_run=False):
        """Rewrite a series

        Args:
            name (str): Name of the series to mark
            series (Series): Series object
            dry_run (bool): True to do a dry run, restoring the original tree
                afterwards

        Return:
            pygit.oid: oid of the new branch
        """
        upstream_name, warn = gitutil.get_upstream(self.gitdir, name)

        repo = pygit2.init_repository(self.gitdir)
        branch = repo.lookup_branch(name)
        upstream = repo.lookup_reference(upstream_name)

        # Checkout the upstream commit in 'detached' mode
        tout.info(f"Checking out upstream commit {upstream.name}")
        commit_oid = upstream.peel(pygit2.GIT_OBJ_COMMIT).oid
        commit = repo.get(commit_oid)
        repo.checkout_tree(commit)
        repo.head.set_target(commit_oid)
        cur = upstream
        tout.info(f"Processing {len(series.commits)} commits from branch '{name}'")
        for cmt in series.commits:
            tout.detail(f"- adding {oid(cmt.hash)} {cmt}")
            repo.cherrypick(cmt.hash)
            if repo.index.conflicts:
                raise ValueError('Conflicts detected')

            tree_id = repo.index.write_tree()
            cherry = repo.get(cmt.hash)
            tout.detail(f"cherry {oid(cherry.oid)}")

            vals.msg = cherry.message
            vals.skip = False
            vals.info = ''
            yield cherry

            repo.create_commit('HEAD', cherry.author, cherry.committer,
                               vals.msg, tree_id, [cur.target])
            cur = repo.head
            repo.state_cleanup()
            tout.info(f"- {vals.info} {oid(cmt.hash)} as {oid(cur.target)}: {cmt}")

        # Update the branch
        target = repo.revparse_single('HEAD')
        tout.info(f"Updating branch {name} to {str(target.oid)[:HASH_LEN]}")
        if dry_run:
            branch_oid = branch.peel(pygit2.GIT_OBJ_COMMIT).oid
            repo.checkout_tree(repo.get(branch_oid))
            repo.head.set_target(branch_oid)
        else:
            repo.create_reference(f'refs/heads/{name}', target.oid, force=True)
        vals.oid = target.oid

    def mark_series(self, name, series, dry_run=False):
        """Mark a series with Change-Id tags

        Args:
            name (str): Name of the series to mark
            series (Series): Series object
            dry_run (bool): True to do a dry run, restoring the original tree
                afterwards

        Return:
            pygit.oid: oid of the new branch
        """
        vals = SimpleNamespace()
        for cherry in self._process_series(vals, name, series, dry_run):
            if CHANGE_ID_TAG not in cherry.message:
                cid = self.make_cid(cherry)
                vals.msg = cherry.message + f'\n{CHANGE_ID_TAG}: {cid}'
                tout.detail(f"   - adding tag")
                vals.info = 'tagged'
            else:
                vals.info = 'has tag'

        return vals.oid

    def unmark_series(self, name, allow_unmarked=False, dry_run=False):
        """Remove Change-Id tags from a series

        Args:
            name (str): Name of the series to unmark
            dry_run (bool): True to do a dry run, restoring the original tree
                afterwards

        Return:
            pygit.oid: oid of the new branch
        """
        name, series, version = self._prep_series(name)
        tout.info(f"Unmarking series '{name}': allow_unmarked {allow_unmarked}")

        if not allow_unmarked:
            bad = []
            for cmt in series.commits:
                if not cmt.change_id:
                    bad.append(cmt)
            if bad:
                print(f'{len(bad)} commit(s) are missing marks')
                for cmt in bad:
                    print(f' - {oid(cmt.hash)} {cmt.subject}')
                raise ValueError(
                    f'Unmarked commits {len(bad)}/{len(series.commits)}')
        vals = SimpleNamespace()
        for cherry in self._process_series(vals, name, series, dry_run):
            if CHANGE_ID_TAG in cherry.message:
                pos = cherry.message.index(CHANGE_ID_TAG)
                cid = self.make_cid(cherry)
                vals.msg = cherry.message[:pos]

                tout.detail(f"   - removing tag")
                vals.info = 'untagged'
            else:
                vals.info = 'no tag'

        if dry_run:
            tout.info('Dry run completed')
        return vals.oid

    def send(self, series):
        """Send out a series

        Args:
            series (str): Name of series to use, or None to use current branch
        """
        ser = self.parse_series(series)
        if not ser.idnum:
            raise ValueError(f"Series '{ser.name}' not found in database")

    def add_upstream(self, name, url):
        """Add a new upstream tree

        Args:
            name (str): Name of the tree
            url (str): URL for the tree
        """
        try:
            res = self.cur.execute(
                f"INSERT INTO upstream (name, url) VALUES ('{name}', '{url}')")
        except sqlite3.IntegrityError as exc:
            if 'UNIQUE constraint failed: upstream.name' in str(exc):
                raise ValueError(f"Upstream '{name}' already exists")
        self.con.commit()

    def list_upstream(self):
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
        res = self.cur.execute(
            f"UPDATE upstream SET is_default = 0")
        if name is not None:
            res = self.cur.execute(
                f"UPDATE upstream SET is_default = 1 WHERE name = '{name}'")
            if self.cur.rowcount != 1:
                self.con.rollback()
                raise ValueError(f"No such upstream '{name}'")
        self.con.commit()

    def get_default_upstream(self):
        """Get the default upstream target

        Return:
            str: Name of the upstream remote to set as default, or None if none
        """
        res = self.cur.execute(
            f"SELECT name FROM upstream WHERE is_default = 1")
        all = res.fetchall()
        if len(all) != 1:
            return None
        return all[0][0]

    def delete_upstream(self, name):
        """Delete an upstream target

        Args:
            name (str): Name of the upstream remote to delete
        """
        res = self.cur.execute(
            f"DELETE FROM upstream WHERE name = '{name}'")
        if self.cur.rowcount != 1:
            self.con.rollback()
            raise ValueError(f"No such upstream '{name}'")
        self.con.commit()

    def remove_series(self, name, dry_run=False):
        """Remove a series from the database

        Args:
            name (str): Name of series to remove, or None to use current one
            dry_run (bool): True to do a dry run
        """
        ser = self.parse_series(name)
        name = ser.name

        res = self.cur.execute(
            f"DELETE FROM series WHERE name = '{name}'")
        if self.cur.rowcount != 1:
            self.con.rollback()
            raise ValueError(f"No such series '{name}'")

        res = self.cur.execute('SELECT id FROM patchwork WHERE series_id = ?',
                               (ser.idnum,))
        all = [str(i) for i in res.fetchall()[0]]
        vals = ', '.join(all[0])
        res = self.cur.execute(f'DELETE FROM pcommit WHERE pwid IN ({vals})')
        res = self.cur.execute('DELETE FROM patchwork WHERE series_id = ?',
                               (ser.idnum,))
        if not dry_run:
            self.con.commit()
        else:
            self.con.rollback()

        self.con.commit()
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

        pwid = self.get_series_pwid(ser.idnum, version)
        res = self.cur.execute(f'DELETE FROM pcommit WHERE pwid = ?', (pwid,))
        res = self.cur.execute(
            'DELETE FROM patchwork WHERE series_id = ? and version = ?',
            (ser.idnum, version))
        if not dry_run:
            self.con.commit()
        else:
            self.con.rollback()

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
        res = self.cur.execute(
            'SELECT id FROM series WHERE '
            f"name = '{name}' AND archived = 0")
        all = res.fetchall()
        if len(all) != 1:
            return None
        return all[0][0]

    def set_project(self, pwork, name):
        """Set the name of the project

        Args:
            pwork (Patchwork): Patchwork object to use
            name (str): Name of the project to use in patchwork
        """
        res = pwork.request(f'projects/')
        pwid = None
        for proj in res:
            if proj['name'] == name:
                pwid = proj['id']
        if not pwid:
            raise ValueError(f"Unknown project name '{name}'")
        res = self.cur.execute(
                f'INSERT INTO settings (name, pwid) VALUES (?, ?)',
                (name, pwid))
        self.con.commit()

    def get_project(self):
        """Get the name of the project

        Returns:
            tuple:
                name (str): Project name, e.g. 'U-Boot'
                pwid (int): Patchworks project ID for this project
        """
        res = self.cur.execute(f"SELECT name, pwid FROM settings")
        all = res.fetchall()
        if len(all) != 1:
            return None
        return all[0]

    def get_project_id(self):
        """Get the patwork ID of the project

        Returns:
            int: ID number

        Raises:
            ValueError: if there is no project
        """
        res = self.get_project()
        if not res:
            raise ValueError('Current project is not known')
        return res[1]

    def list_patches(self, series, version):
        """List patches in a series

        Args:
            series (str): Name of series to use, or None to use current branch
            version (int): Version number, or None to detect from name
        """
        ser, version = self.parse_series_and_version(series, version)
        self.ensure_version(ser, version)
        pwid = self.get_series_pwid(ser.idnum, version)
        pwc = self.get_pcommit_dict_for_pwid(pwid)
        for idnum, (_, subject, _, cid) in pwc.items():
            print(subject)

    def get_series_pwid(self, idnum, version):
        """Get the patchwork ID of a series version

        Args:
            idnum (int): id of the series to look up
            version (int): version number to look up
        """
        res = self.cur.execute(
            f"SELECT id FROM patchwork WHERE series_id = ? AND version = ?",
            (idnum,version))
        all = res.fetchall()
        if not all:
            raise ValueError(f'No matching series for id {idnum}')
        pwid = all[0][0]
        return pwid
