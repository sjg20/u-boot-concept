# -*- coding: utf-8 -*-
# SPDX-License-Identifier:	GPL-2.0+
#
# Copyright 2017 Google, Inc
#

"""Functional tests for checking that patman behaves correctly"""

import aiohttp
import asyncio
import contextlib
import os
import pathlib
import re
import shutil
import sys
import tempfile
import unittest
from unittest import mock

import pygit2
from pygit2.enums import CheckoutStrategy

from u_boot_pylib import cros_subprocess
from u_boot_pylib import gitutil
from u_boot_pylib import terminal
from u_boot_pylib import tools
from u_boot_pylib import tout

from patman import cmdline
from patman.commit import Commit
from patman import control
from patman import cseries
from patman.cseries import PCOMMIT
from patman import database
from patman import patchstream
from patman.patchstream import PatchStream
from patman.patchwork import Patchwork
from patman import send
from patman.series import Series
from patman import settings
from patman import status

PATMAN_DIR = pathlib.Path(__file__).parent
TEST_DATA_DIR = PATMAN_DIR / 'test/'

# Fake patchwork project ID for U-Boot
PROJ_ID = 6
PROJ_LINK_NAME = 'uboot'

# pylint: disable=protected-access


@contextlib.contextmanager
def directory_excursion(directory):
    """Change directory to `directory` for a limited to the context block."""
    current = os.getcwd()
    try:
        os.chdir(directory)
        yield
    finally:
        os.chdir(current)


class Namespace:
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)


class TestFunctional(unittest.TestCase):
    """Functional tests for checking that patman behaves correctly"""
    leb = (b'Lord Edmund Blackadd\xc3\xabr <weasel@blackadder.org>'.
           decode('utf-8'))
    fred = 'Fred Bloggs <f.bloggs@napier.net>'
    joe = 'Joe Bloggs <joe@napierwallies.co.nz>'
    mary = 'Mary Bloggs <mary@napierwallies.co.nz>'
    commits = None
    patches = None
    verbosity = False
    preserve_outdirs = False

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='patman.')
        self.gitdir = os.path.join(self.tmpdir, '.git')
        self.repo = None
        self.cser = None
        self.autolink_extra = None
        tout.init(tout.DEBUG if self.verbosity else tout.INFO,
                  allow_colour=False)
        self.loop = asyncio.get_event_loop()

    def tearDown(self):
        if self.preserve_outdirs:
            print(f'Output dir: {self.tmpdir}')
        else:
            shutil.rmtree(self.tmpdir)
        terminal.set_print_test_mode(False)

    @classmethod
    def setup_test_args(cls, preserve_indir=False, preserve_outdirs=False,
                        toolpath=None, verbosity=None, no_capture=False):
        """Accept arguments controlling test execution

        Args:
            preserve_indir: not used
            preserve_outdir: Preserve the output directories used by tests. Each
                test has its own, so this is normally only useful when running a
                single test.
            toolpath: not used
        """
        cls.preserve_outdirs = preserve_outdirs
        cls.toolpath = toolpath
        cls.verbosity = verbosity
        cls.no_capture = no_capture

    @staticmethod
    def _get_path(fname):
        """Get the path to a test file

        Args:
            fname (str): Filename to obtain

        Returns:
            str: Full path to file in the test directory
        """
        return TEST_DATA_DIR / fname

    @classmethod
    def _get_text(cls, fname):
        """Read a file as textget_branch

        Args:
            fname (str): Filename to read

        Returns:
            str: Contents of file
        """
        return open(cls._get_path(fname), encoding='utf-8').read()

    @classmethod
    def _get_patch_name(cls, subject):
        """Get the filename of a patch given its subject

        Args:
            subject (str): Patch subject

        Returns:
            str: Filename for that patch
        """
        fname = re.sub('[ :]', '-', subject)
        return fname.replace('--', '-')

    def _create_patches_for_test(self, series):
        """Create patch files for use by tests

        This copies patch files from the test directory as needed by the series

        Args:
            series (Series): Series containing commits to convert

        Returns:
            tuple:
                str: Cover-letter filename, or None if none
                fname_list: list of str, each a patch filename
        """
        cover_fname = None
        fname_list = []
        for i, commit in enumerate(series.commits):
            clean_subject = self._get_patch_name(commit.subject)
            src_fname = '%04d-%s.patch' % (i + 1, clean_subject[:52])
            fname = os.path.join(self.tmpdir, src_fname)
            shutil.copy(self._get_path(src_fname), fname)
            fname_list.append(fname)
        if series.get('cover'):
            src_fname = '0000-cover-letter.patch'
            cover_fname = os.path.join(self.tmpdir, src_fname)
            fname = os.path.join(self.tmpdir, src_fname)
            shutil.copy(self._get_path(src_fname), fname)

        return cover_fname, fname_list

    def assertFinished(self, itr):
        """Assert that an iterator is finished

        Args:
            itr (iter): Iterator to check
        """
        val = [x for x in itr]
        self.assertFalse(val)

    def test_basic(self):
        """Tests the basic flow of patman

        This creates a series from some hard-coded patches build from a simple
        tree with the following metadata in the top commit:

            Series-to: u-boot
            Series-prefix: RFC
            Series-postfix: some-branch
            Series-cc: Stefan Brüns <stefan.bruens@rwth-aachen.de>
            Cover-letter-cc: Lord Mëlchett <clergy@palace.gov>
            Series-version: 3
            Patch-cc: fred
            Series-process-log: sort, uniq
            Series-changes: 4
            - Some changes
            - Multi
              line
              change

            Commit-changes: 2
            - Changes only for this commit

            Cover-changes: 4
            - Some notes for the cover letter

            Cover-letter:
            test: A test patch series
            This is a test of how the cover
            letter
            works
            END

        and this in the first commit:

            Commit-changes: 2
            - second revision change

            Series-notes:
            some notes
            about some things
            from the first commit
            END

            Commit-notes:
            Some notes about
            the first commit
            END

        with the following commands:

           git log -n2 --reverse >/path/to/tools/patman/test/test01.txt
           git format-patch --subject-prefix RFC --cover-letter HEAD~2
           mv 00* /path/to/tools/patman/test

        It checks these aspects:
            - git log can be processed by patchstream
            - emailing patches uses the correct command
            - CC file has information on each commit
            - cover letter has the expected text and subject
            - each patch has the correct subject
            - dry-run information prints out correctly
            - unicode is handled correctly
            - Series-to, Series-cc, Series-prefix, Series-postfix, Cover-letter
            - Cover-letter-cc, Series-version, Series-changes, Series-notes
            - Commit-notes
        """
        process_tags = True
        ignore_bad_tags = False
        stefan = b'Stefan Br\xc3\xbcns <stefan.bruens@rwth-aachen.de>'.decode('utf-8')
        rick = 'Richard III <richard@palace.gov>'
        mel = b'Lord M\xc3\xablchett <clergy@palace.gov>'.decode('utf-8')
        add_maintainers = [stefan, rick]
        dry_run = True
        in_reply_to = mel
        count = 2
        settings.alias = {
            'fdt': ['simon'],
            'u-boot': ['u-boot@lists.denx.de'],
            'simon': [self.leb],
            'fred': [self.fred],
            'joe': [self.joe],
        }

        text = self._get_text('test01.txt')
        series = patchstream.get_metadata_for_test(text)
        series.base_commit = Commit('1a44532')
        series.branch = 'mybranch'
        cover_fname, args = self._create_patches_for_test(series)
        get_maintainer_script = str(pathlib.Path(__file__).parent.parent.parent
                                    / 'get_maintainer.pl') + ' --norolestats'
        with terminal.capture() as out:
            patchstream.fix_patches(series, args)
            if cover_fname and series.get('cover'):
                patchstream.insert_cover_letter(cover_fname, series, count)
            series.DoChecks()
            cc_file = series.MakeCcFile(process_tags, cover_fname,
                                        not ignore_bad_tags, add_maintainers,
                                        None, get_maintainer_script)
            cmd = gitutil.email_patches(
                series, cover_fname, args, dry_run, not ignore_bad_tags,
                cc_file, in_reply_to=in_reply_to, thread=None)
            series.ShowActions(args, cmd, process_tags)
        cc_lines = open(cc_file, encoding='utf-8').read().splitlines()
        os.remove(cc_file)

        lines = iter(out[0].getvalue().splitlines())
        self.assertEqual('Cleaned %s patches' % len(series.commits),
                         next(lines))
        self.assertEqual('Change log missing for v2', next(lines))
        self.assertEqual('Change log missing for v3', next(lines))
        self.assertEqual('Change log for unknown version v4', next(lines))
        self.assertEqual("Alias 'pci' not found", next(lines))
        while next(lines) != 'Cc processing complete':
            pass
        self.assertIn('Dry run', next(lines))
        self.assertEqual('', next(lines))
        self.assertIn('Send a total of %d patches' % count, next(lines))
        prev = next(lines)
        for i, commit in enumerate(series.commits):
            self.assertEqual('   %s' % args[i], prev)
            while True:
                prev = next(lines)
                if 'Cc:' not in prev:
                    break
        self.assertEqual('To:	  u-boot@lists.denx.de', prev)
        self.assertEqual('Cc:	  %s' % stefan, next(lines))
        self.assertEqual('Version:  3', next(lines))
        self.assertEqual('Prefix:\t  RFC', next(lines))
        self.assertEqual('Postfix:\t  some-branch', next(lines))
        self.assertEqual('Cover: 4 lines', next(lines))
        self.assertEqual('      Cc:  %s' % self.fred, next(lines))
        self.assertEqual('      Cc:  %s' % self.joe, next(lines))
        self.assertEqual('      Cc:  %s' % self.leb,
                         next(lines))
        self.assertEqual('      Cc:  %s' % mel, next(lines))
        self.assertEqual('      Cc:  %s' % rick, next(lines))
        expected = ('Git command: git send-email --annotate '
                    '--in-reply-to="%s" --to u-boot@lists.denx.de '
                    '--cc "%s" --cc-cmd "%s send --cc-cmd %s" %s %s'
                    % (in_reply_to, stefan, sys.argv[0], cc_file, cover_fname,
                       ' '.join(args)))
        self.assertEqual(expected, next(lines))

        self.assertEqual(('%s %s\0%s' % (args[0], rick, stefan)), cc_lines[0])
        self.assertEqual(
            '%s %s\0%s\0%s\0%s\0%s' % (args[1], self.fred, self.joe, self.leb,
                                       rick, stefan),
            cc_lines[1])

        expected = '''
This is a test of how the cover
letter
works

some notes
about some things
from the first commit

Changes in v4:
- Multi
  line
  change
- Some changes
- Some notes for the cover letter
- fdt: Correct cast for sandbox in fdtdec_setup_mem_size_base()

Simon Glass (2):
  pci: Correct cast for sandbox
  fdt: Correct cast for sandbox in fdtdec_setup_mem_size_base()

 cmd/pci.c                   | 3 ++-
 fs/fat/fat.c                | 1 +
 lib/efi_loader/efi_memory.c | 1 +
 lib/fdtdec.c                | 3 ++-
 4 files changed, 6 insertions(+), 2 deletions(-)

--\x20
2.7.4

base-commit: 1a44532
branch: mybranch
'''
        lines = open(cover_fname, encoding='utf-8').read().splitlines()
        self.assertEqual(
            'Subject: [RFC PATCH some-branch v3 0/2] test: A test patch series',
            lines[3])
        self.assertEqual(expected.splitlines(), lines[7:])

        for i, fname in enumerate(args):
            lines = open(fname, encoding='utf-8').read().splitlines()
            subject = [line for line in lines if line.startswith('Subject')]
            self.assertEqual('Subject: [RFC %d/%d]' % (i + 1, count),
                             subject[0][:18])

            # Check that we got our commit notes
            start = 0
            expected = ''

            if i == 0:
                start = 17
                expected = '''---
Some notes about
the first commit

(no changes since v2)

Changes in v2:
- second revision change'''
            elif i == 1:
                start = 17
                expected = '''---

Changes in v4:
- Multi
  line
  change
- New
- Some changes

Changes in v2:
- Changes only for this commit'''

            if expected:
                expected = expected.splitlines()
                self.assertEqual(expected, lines[start:(start+len(expected))])

    def test_base_commit(self):
        """Test adding a base commit with no cover letter"""
        orig_text = self._get_text('test01.txt')
        pos = orig_text.index('commit 5ab48490f03051875ab13d288a4bf32b507d76fd')
        text = orig_text[:pos]
        series = patchstream.get_metadata_for_test(text)
        series.base_commit = Commit('1a44532')
        series.branch = 'mybranch'
        cover_fname, args = self._create_patches_for_test(series)
        self.assertFalse(cover_fname)
        with terminal.capture() as out:
            patchstream.fix_patches(series, args, insert_base_commit=True)
        self.assertEqual('Cleaned 1 patch\n', out[0].getvalue())
        lines = tools.read_file(args[0], binary=False).splitlines()
        pos = lines.index('-- ')

        # We expect these lines at the end:
        # -- (with trailing space)
        # 2.7.4
        # (empty)
        # base-commit: xxx
        # branch: xxx
        self.assertEqual('base-commit: 1a44532', lines[pos + 3])
        self.assertEqual('branch: mybranch', lines[pos + 4])

    def _check_dirty(self, repo):
        """Check if the tree is dirty

        Args:
            repo (pygit2.repo): Repo to use

        Return:
            bool: True if the tree is dirty, False if clean
        """
        state = repo.status(untracked_files='no')
        return bool(state)

    def make_commit_with_file(self, subject, body, fname, text):
        """Create a file and add it to the git repo with a new commit

        Args:
            subject (str): Subject for the commit
            body (str): Body text of the commit
            fname (str): Filename of file to create
            text (str): Text to put into the file
        """
        path = os.path.join(self.tmpdir, fname)
        tools.write_file(path, text, binary=False)
        index = self.repo.index
        index.add(fname)
        # pylint doesn't seem to find this
        # pylint: disable=E1101
        author = pygit2.Signature('Test user', 'test@email.com')
        committer = author
        tree = index.write_tree()
        message = subject + '\n' + body
        self.repo.create_commit('HEAD', author, committer, message, tree,
                                [self.repo.head.target])

    def make_git_tree(self):
        """Make a simple git tree suitable for testing

        It has four branches:
            'base' has two commits: PCI, main
            'first' has base as upstream and two more commits: I2C, SPI
            'second' has base as upstream and three more: video, serial, bootm
            'third4' has second as upstream and four more: usb, main, test, lib

        Returns:
            pygit2.Repository: repository
        """
        # tools.write_file(os.path.join(self.tmpdir, '.gitconfig'), '''[user]
        #    name = Test User
        #    email = me@test.com
        #    ''', binary=False)
        # cfg = os.path.join(self.tmpdir, '.gitconfig')
        os.environ['GIT_CONFIG_GLOBAL'] = '/dev/null'
        os.environ['GIT_CONFIG_SYSTEM'] = '/dev/null'

        repo = pygit2.init_repository(self.gitdir)
        self.repo = repo
        new_tree = repo.TreeBuilder().write()

        common = ['git', f'--git-dir={self.gitdir}', 'config']
        tools.run(*(common + ['user.name', 'Dummy']), cwd=self.gitdir)
        tools.run(*(common + ['user.email', 'dumdum@dummy.com']),
                  cwd=self.gitdir)

        # pylint doesn't seem to find this
        # pylint: disable=E1101
        author = pygit2.Signature('Test user', 'test@email.com')
        committer = author
        _ = repo.create_commit('HEAD', author, committer, 'Created master',
                               new_tree, [])

        self.make_commit_with_file('Initial commit', '''
Add a README

''', 'README', '''This is the README file
describing this project
in very little detail''')

        self.make_commit_with_file('pci: PCI implementation', '''
Here is a basic PCI implementation

''', 'pci.c', '''This is a file
it has some contents
and some more things''')
        self.make_commit_with_file('main: Main program', '''
Hello here is the second commit.
''', 'main.c', '''This is the main file
there is very little here
but we can always add more later
if we want to

Series-to: u-boot
Series-cc: Barry Crump <bcrump@whataroa.nz>
''')
        base_target = repo.revparse_single('HEAD')
        self.make_commit_with_file('i2c: I2C things', '''
This has some stuff to do with I2C
''', 'i2c.c', '''And this is the file contents
with some I2C-related things in it''')
        self.make_commit_with_file('spi: SPI fixes', '''
SPI needs some fixes
and here they are

Signed-off-by: %s

Series-to: u-boot
Commit-notes:
title of the series
This is the cover letter for the series
with various details
END
''' % self.leb, 'spi.c', '''Some fixes for SPI in this
file to make SPI work
better than before''')
        first_target = repo.revparse_single('HEAD')

        target = repo.revparse_single('HEAD~2')
        # pylint doesn't seem to find this
        # pylint: disable=E1101
        repo.reset(target.oid, pygit2.enums.ResetMode.HARD)
        self.make_commit_with_file('video: Some video improvements', '''
Fix up the video so that
it looks more purple. Purple is
a very nice colour.
''', 'video.c', '''More purple here
Purple and purple
Even more purple
Could not be any more purple''')
        self.make_commit_with_file('serial: Add a serial driver', '''
Here is the serial driver
for my chip.

Cover-letter:
Series for my board
This series implements support
for my glorious board.
END
Series-to: u-boot
Series-links: 183237
''', 'serial.c', '''The code for the
serial driver is here''')
        self.make_commit_with_file('bootm: Make it boot', '''
This makes my board boot
with a fix to the bootm
command
''', 'bootm.c', '''Fix up the bootm
command to make the code as
complicated as possible''')
        second_target = repo.revparse_single('HEAD')

        self.make_commit_with_file('usb: Try out the new DMA feature', '''
This is just a fix that
ensures that DMA is enabled
''', 'usb-uclass.c', '''Here is the USB
implementation and as you can see it
it very nice''')
        self.make_commit_with_file('main: Change to the main program', '''
Here we adjust the main
program just a little bit
''', 'main.c', '''This is the text of the main program''')
        self.make_commit_with_file('test: Check that everything works', '''
This checks that all the
various things we've been
adding actually work.
''', 'test.c', '''Here is the test code and it seems OK''')
        self.make_commit_with_file('lib: Sort out the extra library', '''
The extra library is currently
broken. Fix it so that we can
use it in various place.
''', 'lib.c', '''Some library code is here
and a little more''')
        third_target = repo.revparse_single('HEAD')

        repo.branches.local.create('first', first_target)
        repo.config.set_multivar('branch.first.remote', '', '.')
        repo.config.set_multivar('branch.first.merge', '', 'refs/heads/base')

        repo.branches.local.create('second', second_target)
        repo.config.set_multivar('branch.second.remote', '', '.')
        repo.config.set_multivar('branch.second.merge', '', 'refs/heads/base')

        repo.branches.local.create('base', base_target)

        repo.branches.local.create('third4', third_target)
        repo.config.set_multivar('branch.third4.remote', '', '.')
        repo.config.set_multivar('branch.third4.merge', '', 'refs/heads/second')

        target = repo.lookup_reference('refs/heads/first')
        repo.checkout(target, strategy=pygit2.GIT_CHECKOUT_FORCE)
        target = repo.revparse_single('HEAD')
        repo.reset(target.oid, pygit2.enums.ResetMode.HARD)

        self.assertFalse(gitutil.check_dirty(self.gitdir, self.tmpdir))
        return repo

    def test_branch(self):
        """Test creating patches from a branch"""
        repo = self.make_git_tree()
        target = repo.lookup_reference('refs/heads/first')
        # pylint doesn't seem to find this
        # pylint: disable=E1101
        self.repo.checkout(target, strategy=pygit2.GIT_CHECKOUT_FORCE)
        control.setup()
        orig_dir = os.getcwd()
        try:
            os.chdir(self.tmpdir)

            # Check that it can detect the current branch
            self.assertEqual(2, gitutil.count_commits_to_branch(None))
            col = terminal.Color()
            with terminal.capture() as _:
                _, cover_fname, patch_files = send.prepare_patches(
                    col, branch=None, count=-1, start=0, end=0,
                    ignore_binary=False, signoff=True)
            self.assertIsNone(cover_fname)
            self.assertEqual(2, len(patch_files))

            # Check that it can detect a different branch
            self.assertEqual(3, gitutil.count_commits_to_branch('second'))
            with terminal.capture() as _:
                series, cover_fname, patch_files = send.prepare_patches(
                    col, branch='second', count=-1, start=0, end=0,
                    ignore_binary=False, signoff=True)
            self.assertIsNotNone(cover_fname)
            self.assertEqual(3, len(patch_files))

            cover = tools.read_file(cover_fname, binary=False)
            lines = cover.splitlines()[-2:]
            base = repo.lookup_reference('refs/heads/base').target
            self.assertEqual(f'base-commit: {base}', lines[0])
            self.assertEqual('branch: second', lines[1])

            # Make sure that the base-commit is not present when it is in the
            # cover letter
            for fname in patch_files:
                self.assertNotIn(b'base-commit:', tools.read_file(fname))

            # Check that it can skip patches at the end
            with terminal.capture() as _:
                _, cover_fname, patch_files = send.prepare_patches(
                    col, branch='second', count=-1, start=0, end=1,
                    ignore_binary=False, signoff=True)
            self.assertIsNotNone(cover_fname)
            self.assertEqual(2, len(patch_files))

            cover = tools.read_file(cover_fname, binary=False)
            lines = cover.splitlines()[-2:]
            base2 = repo.lookup_reference('refs/heads/second')
            ref = base2.peel(pygit2.GIT_OBJ_COMMIT).parents[0].parents[0].id
            self.assertEqual(f'base-commit: {ref}', lines[0])
            self.assertEqual('branch: second', lines[1])
        finally:
            os.chdir(orig_dir)

    def test_custom_get_maintainer_script(self):
        """Validate that a custom get_maintainer script gets used."""
        self.make_git_tree()
        with directory_excursion(self.tmpdir):
            # Setup git.
            os.environ['GIT_CONFIG_GLOBAL'] = '/dev/null'
            os.environ['GIT_CONFIG_SYSTEM'] = '/dev/null'
            tools.run('git', 'config', 'user.name', 'Dummy')
            tools.run('git', 'config', 'user.email', 'dumdum@dummy.com')
            tools.run('git', 'branch', 'upstream')
            tools.run('git', 'branch', '--set-upstream-to=upstream')

            # Setup patman configuration.
            with open('.patman', 'w', buffering=1) as f:
                f.write('[settings]\n'
                        'get_maintainer_script: dummy-script.sh\n'
                        'check_patch: False\n'
                        'add_maintainers: True\n')
            with open('dummy-script.sh', 'w', buffering=1) as f:
                f.write('#!/usr/bin/env python\n'
                        'print("hello@there.com")\n')
            os.chmod('dummy-script.sh', 0x555)
            tools.run('git', 'add', '.')
            tools.run('git', 'commit', '-m', 'new commit')

            # Finally, do the test
            with terminal.capture():
                output = tools.run(PATMAN_DIR / 'patman', 'send', '--dry-run')
                # Assert the email address is part of the dry-run
                # output.
                self.assertIn('hello@there.com', output)

    def test_tags(self):
        """Test collection of tags in a patchstream"""
        text = '''This is a patch

Signed-off-by: Terminator
Reviewed-by: %s
Reviewed-by: %s
Tested-by: %s
''' % (self.joe, self.mary, self.leb)
        pstrm = PatchStream.process_text(text)
        self.assertEqual(pstrm.commit.rtags, {
            'Reviewed-by': {self.joe, self.mary},
            'Tested-by': {self.leb}})

    def test_invalid_tag(self):
        """Test invalid tag in a patchstream"""
        text = '''This is a patch

Serie-version: 2
'''
        with self.assertRaises(ValueError) as exc:
            pstrm = PatchStream.process_text(text)
        self.assertEqual("Line 3: Invalid tag = 'Serie-version: 2'",
                         str(exc.exception))

    def test_missing_end(self):
        """Test a missing END tag"""
        text = '''This is a patch

Cover-letter:
This is the title
missing END after this line
Signed-off-by: Fred
'''
        pstrm = PatchStream.process_text(text)
        self.assertEqual(["Missing 'END' in section 'cover'"],
                         pstrm.commit.warn)

    def test_missing_blank_line(self):
        """Test a missing blank line after a tag"""
        text = '''This is a patch

Series-changes: 2
- First line of changes
- Missing blank line after this line
Signed-off-by: Fred
'''
        pstrm = PatchStream.process_text(text)
        self.assertEqual(["Missing 'blank line' in section 'Series-changes'"],
                         pstrm.commit.warn)

    def test_invalid_commit_tag(self):
        """Test an invalid Commit-xxx tag"""
        text = '''This is a patch

Commit-fred: testing
'''
        pstrm = PatchStream.process_text(text)
        self.assertEqual(["Line 3: Ignoring Commit-fred"], pstrm.commit.warn)

    def test_self_test(self):
        """Test a tested by tag by this user"""
        test_line = 'Tested-by: %s@napier.com' % os.getenv('USER')
        text = '''This is a patch

%s
''' % test_line
        pstrm = PatchStream.process_text(text)
        self.assertEqual(["Ignoring '%s'" % test_line], pstrm.commit.warn)

    def test_space_before_tab(self):
        """Test a space before a tab"""
        text = '''This is a patch

+ \tSomething
'''
        pstrm = PatchStream.process_text(text)
        self.assertEqual(["Line 3/0 has space before tab"], pstrm.commit.warn)

    def test_lines_after_test(self):
        """Test detecting lines after TEST= line"""
        text = '''This is a patch

TEST=sometest
more lines
here
'''
        pstrm = PatchStream.process_text(text)
        self.assertEqual(["Found 2 lines after TEST="], pstrm.commit.warn)

    def test_blank_line_at_end(self):
        """Test detecting a blank line at the end of a file"""
        text = '''This is a patch

diff --git a/lib/fdtdec.c b/lib/fdtdec.c
index c072e54..942244f 100644
--- a/lib/fdtdec.c
+++ b/lib/fdtdec.c
@@ -1200,7 +1200,8 @@ int fdtdec_setup_mem_size_base(void)
 	}

 	gd->ram_size = (phys_size_t)(res.end - res.start + 1);
-	debug("%s: Initial DRAM size %llx\n", __func__, (u64)gd->ram_size);
+	debug("%s: Initial DRAM size %llx\n", __func__,
+	      (unsigned long long)gd->ram_size);
+
diff --git a/lib/efi_loader/efi_memory.c b/lib/efi_loader/efi_memory.c

--
2.7.4

 '''
        pstrm = PatchStream.process_text(text)
        self.assertEqual(
            ["Found possible blank line(s) at end of file 'lib/fdtdec.c'"],
            pstrm.commit.warn)

    def test_no_upstream(self):
        """Test CountCommitsToBranch when there is no upstream"""
        repo = self.make_git_tree()
        target = repo.lookup_reference('refs/heads/base')
        # pylint doesn't seem to find this
        # pylint: disable=E1101
        self.repo.checkout(target, strategy=pygit2.GIT_CHECKOUT_FORCE)

        # Check that it can detect the current branch
        orig_dir = os.getcwd()
        try:
            os.chdir(self.gitdir)
            with self.assertRaises(ValueError) as exc:
                gitutil.count_commits_to_branch(None)
            self.assertIn(
                "Failed to determine upstream: fatal: no upstream configured for branch 'base'",
                str(exc.exception))
        finally:
            os.chdir(orig_dir)

    @staticmethod
    def _fake_patchwork(subpath):
        """Fake Patchwork server for the function below

        This handles accessing a series, providing a list consisting of a
        single patch

        Args:
            subpath (str): URL subpath to use
        """
        re_series = re.match(r'series/(\d*)/$', subpath)
        if re_series:
            series_num = re_series.group(1)
            if series_num == '1234':
                return {'patches': [
                    {'id': '1', 'name': 'Some patch'}]}
        raise ValueError('Fake Patchwork does not understand: %s' % subpath)

    def test_status_mismatch(self):
        """Test Patchwork patches not matching the series"""
        series = Series()

        pwork = Patchwork.for_testing(self._fake_patchwork)

        with terminal.capture() as (_, err):
            status.collect_patches(series, 1234, pwork, False)
        self.assertIn('Warning: Patchwork reports 1 patches, series has 0',
                      err.getvalue())

    def test_status_read_patch(self):
        """Test handling a single patch in Patchwork"""
        series = Series()
        series.commits = [Commit('abcd')]

        pwork = Patchwork.for_testing(self._fake_patchwork)

        patches, _ = status.collect_patches(series, 1234, pwork, False)
        self.assertEqual(1, len(patches))
        patch = patches[0]
        self.assertEqual('1', patch.id)
        self.assertEqual('Some patch', patch.raw_subject)

    def test_parse_subject(self):
        """Test parsing of the patch subject"""
        patch = status.Patch('1')

        # Simple patch not in a series
        patch.parse_subject('Testing')
        self.assertEqual('Testing', patch.raw_subject)
        self.assertEqual('Testing', patch.subject)
        self.assertEqual(1, patch.seq)
        self.assertEqual(1, patch.count)
        self.assertEqual(None, patch.prefix)
        self.assertEqual(None, patch.version)

        # First patch in a series
        patch.parse_subject('[1/2] Testing')
        self.assertEqual('[1/2] Testing', patch.raw_subject)
        self.assertEqual('Testing', patch.subject)
        self.assertEqual(1, patch.seq)
        self.assertEqual(2, patch.count)
        self.assertEqual(None, patch.prefix)
        self.assertEqual(None, patch.version)

        # Second patch in a series
        patch.parse_subject('[2/2] Testing')
        self.assertEqual('Testing', patch.subject)
        self.assertEqual(2, patch.seq)
        self.assertEqual(2, patch.count)
        self.assertEqual(None, patch.prefix)
        self.assertEqual(None, patch.version)

        # With PATCH prefix
        patch.parse_subject('[PATCH,2/5] Testing')
        self.assertEqual('Testing', patch.subject)
        self.assertEqual(2, patch.seq)
        self.assertEqual(5, patch.count)
        self.assertEqual('PATCH', patch.prefix)
        self.assertEqual(None, patch.version)

        # RFC patch
        patch.parse_subject('[RFC,3/7] Testing')
        self.assertEqual('Testing', patch.subject)
        self.assertEqual(3, patch.seq)
        self.assertEqual(7, patch.count)
        self.assertEqual('RFC', patch.prefix)
        self.assertEqual(None, patch.version)

        # Version patch
        patch.parse_subject('[v2,3/7] Testing')
        self.assertEqual('Testing', patch.subject)
        self.assertEqual(3, patch.seq)
        self.assertEqual(7, patch.count)
        self.assertEqual(None, patch.prefix)
        self.assertEqual('v2', patch.version)

        # All fields
        patch.parse_subject('[RESEND,v2,3/7] Testing')
        self.assertEqual('Testing', patch.subject)
        self.assertEqual(3, patch.seq)
        self.assertEqual(7, patch.count)
        self.assertEqual('RESEND', patch.prefix)
        self.assertEqual('v2', patch.version)

        # RFC only
        patch.parse_subject('[RESEND] Testing')
        self.assertEqual('Testing', patch.subject)
        self.assertEqual(1, patch.seq)
        self.assertEqual(1, patch.count)
        self.assertEqual('RESEND', patch.prefix)
        self.assertEqual(None, patch.version)

    def test_compare_series(self):
        """Test operation of compare_with_series()"""
        commit1 = Commit('abcd')
        commit1.subject = 'Subject 1'
        commit2 = Commit('ef12')
        commit2.subject = 'Subject 2'
        commit3 = Commit('3456')
        commit3.subject = 'Subject 2'

        patch1 = status.Patch('1')
        patch1.subject = 'Subject 1'
        patch2 = status.Patch('2')
        patch2.subject = 'Subject 2'
        patch3 = status.Patch('3')
        patch3.subject = 'Subject 2'

        series = Series()
        series.commits = [commit1]
        patches = [patch1]
        patch_for_commit, commit_for_patch, warnings = (
            status.compare_with_series(series, patches))
        self.assertEqual(1, len(patch_for_commit))
        self.assertEqual(patch1, patch_for_commit[0])
        self.assertEqual(1, len(commit_for_patch))
        self.assertEqual(commit1, commit_for_patch[0])

        series.commits = [commit1]
        patches = [patch1, patch2]
        patch_for_commit, commit_for_patch, warnings = (
            status.compare_with_series(series, patches))
        self.assertEqual(1, len(patch_for_commit))
        self.assertEqual(patch1, patch_for_commit[0])
        self.assertEqual(1, len(commit_for_patch))
        self.assertEqual(commit1, commit_for_patch[0])
        self.assertEqual(["Cannot find commit for patch 2 ('Subject 2')"],
                         warnings)

        series.commits = [commit1, commit2]
        patches = [patch1]
        patch_for_commit, commit_for_patch, warnings = (
            status.compare_with_series(series, patches))
        self.assertEqual(1, len(patch_for_commit))
        self.assertEqual(patch1, patch_for_commit[0])
        self.assertEqual(1, len(commit_for_patch))
        self.assertEqual(commit1, commit_for_patch[0])
        self.assertEqual(["Cannot find patch for commit 2 ('Subject 2')"],
                         warnings)

        series.commits = [commit1, commit2, commit3]
        patches = [patch1, patch2]
        patch_for_commit, commit_for_patch, warnings = (
            status.compare_with_series(series, patches))
        self.assertEqual(2, len(patch_for_commit))
        self.assertEqual(patch1, patch_for_commit[0])
        self.assertEqual(patch2, patch_for_commit[1])
        self.assertEqual(1, len(commit_for_patch))
        self.assertEqual(commit1, commit_for_patch[0])
        self.assertEqual(["Cannot find patch for commit 3 ('Subject 2')",
                          "Multiple commits match patch 2 ('Subject 2'):\n"
                          '   Subject 2\n   Subject 2'],
                         warnings)

        series.commits = [commit1, commit2]
        patches = [patch1, patch2, patch3]
        patch_for_commit, commit_for_patch, warnings = (
            status.compare_with_series(series, patches))
        self.assertEqual(1, len(patch_for_commit))
        self.assertEqual(patch1, patch_for_commit[0])
        self.assertEqual(2, len(commit_for_patch))
        self.assertEqual(commit1, commit_for_patch[0])
        self.assertEqual(["Multiple patches match commit 2 ('Subject 2'):\n"
                          '   Subject 2\n   Subject 2',
                          "Cannot find commit for patch 3 ('Subject 2')"],
                         warnings)

    def _fake_patchwork2(self, subpath):
        """Fake Patchwork server for the function below

        This handles accessing series, patches and comments, providing the data
        in self.patches to the caller

        Args:
            url (str): URL of patchwork server
            subpath (str): URL subpath to use
        """
        re_series = re.match(r'series/(\d*)/$', subpath)
        re_patch = re.match(r'patches/(\d*)/$', subpath)
        re_comments = re.match(r'patches/(\d*)/comments/$', subpath)
        if re_series:
            series_num = re_series.group(1)
            if series_num == '1234':
                return {'patches': self.patches}
        elif re_patch:
            patch_num = int(re_patch.group(1))
            patch = self.patches[patch_num - 1]
            return patch
        elif re_comments:
            patch_num = int(re_comments.group(1))
            patch = self.patches[patch_num - 1]
            return patch.comments
        raise ValueError('Fake Patchwork does not understand: %s' % subpath)

    def test_find_new_responses(self):
        """Test operation of find_new_responses()"""
        commit1 = Commit('abcd')
        commit1.subject = 'Subject 1'
        commit2 = Commit('ef12')
        commit2.subject = 'Subject 2'

        patch1 = status.Patch('1')
        patch1.parse_subject('[1/2] Subject 1')
        patch1.name = patch1.raw_subject
        patch1.content = 'This is my patch content'
        comment1a = {'content': 'Reviewed-by: %s\n' % self.joe}

        patch1.comments = [comment1a]

        patch2 = status.Patch('2')
        patch2.parse_subject('[2/2] Subject 2')
        patch2.name = patch2.raw_subject
        patch2.content = 'Some other patch content'
        comment2a = {
            'content': 'Reviewed-by: %s\nTested-by: %s\n' %
                       (self.mary, self.leb)}
        comment2b = {'content': 'Reviewed-by: %s' % self.fred}
        patch2.comments = [comment2a, comment2b]

        # This test works by setting up commits and patch for use by the fake
        # Rest API function _fake_patchwork2(). It calls various functions in
        # the status module after setting up tags in the commits, checking that
        # things behaves as expected
        self.commits = [commit1, commit2]
        self.patches = [patch1, patch2]
        count = 2
        new_rtag_list = [None] * count
        review_list = [None, None]

        # Check that the tags are picked up on the first patch
        pwork = Patchwork.for_testing(self._fake_patchwork2)
        new_rtag_list[0], review_list[0] = status.find_new_responses(
            commit1, patch1, pwork)
        self.assertEqual(new_rtag_list[0], {'Reviewed-by': {self.joe}})

        # Now the second patch
        new_rtag_list[1], review_list[1] = status.find_new_responses(
            commit2, patch2, pwork)
        self.assertEqual(new_rtag_list[1], {
            'Reviewed-by': {self.mary, self.fred},
            'Tested-by': {self.leb}})

        # Now add some tags to the commit, which means they should not appear as
        # 'new' tags when scanning comments
        new_rtag_list = [None] * count
        commit1.rtags = {'Reviewed-by': {self.joe}}
        new_rtag_list[0], review_list[0] = status.find_new_responses(
            commit1, patch1, pwork)
        self.assertEqual(new_rtag_list[0], {})

        # For the second commit, add Ed and Fred, so only Mary should be left
        commit2.rtags = {
            'Tested-by': {self.leb},
            'Reviewed-by': {self.fred}}
        new_rtag_list[1], review_list[1] = status.find_new_responses(
            commit2, patch2, pwork)
        self.assertEqual(new_rtag_list[1], {'Reviewed-by': {self.mary}})

        # Check that the output patches expectations:
        #   1 Subject 1
        #     Reviewed-by: Joe Bloggs <joe@napierwallies.co.nz>
        #   2 Subject 2
        #     Tested-by: Lord Edmund Blackaddër <weasel@blackadder.org>
        #     Reviewed-by: Fred Bloggs <f.bloggs@napier.net>
        #   + Reviewed-by: Mary Bloggs <mary@napierwallies.co.nz>
        # 1 new response available in patchwork

        series = Series()
        series.commits = [commit1, commit2]
        terminal.set_print_test_mode()
        status.check_patchwork_status(
            series, '1234', None, None, False, False, False, pwork)
        lines = iter(terminal.get_print_test_lines())
        col = terminal.Color()
        self.assertEqual(terminal.PrintLine('  1 Subject 1', col.YELLOW),
                         next(lines))
        self.assertEqual(
            terminal.PrintLine('    Reviewed-by: ', col.GREEN, newline=False,
                               bright=False),
            next(lines))
        self.assertEqual(terminal.PrintLine(self.joe, col.WHITE, bright=False),
                         next(lines))

        self.assertEqual(terminal.PrintLine('  2 Subject 2', col.YELLOW),
                         next(lines))
        self.assertEqual(
            terminal.PrintLine('    Reviewed-by: ', col.GREEN, newline=False,
                               bright=False),
            next(lines))
        self.assertEqual(terminal.PrintLine(self.fred, col.WHITE, bright=False),
                         next(lines))
        self.assertEqual(
            terminal.PrintLine('    Tested-by: ', col.GREEN, newline=False,
                               bright=False),
            next(lines))
        self.assertEqual(terminal.PrintLine(self.leb, col.WHITE, bright=False),
                         next(lines))
        self.assertEqual(
            terminal.PrintLine('  + Reviewed-by: ', col.GREEN, newline=False),
            next(lines))
        self.assertEqual(terminal.PrintLine(self.mary, col.WHITE),
                         next(lines))
        self.assertEqual(terminal.PrintLine(
            '1 new response available in patchwork (use -d to write them to a new branch)',
            None), next(lines))

    def _fake_patchwork3(self, subpath):
        """Fake Patchwork server for the function below

        This handles accessing series, patches and comments, providing the data
        in self.patches to the caller

        Args:
            subpath (str): URL subpath to use
        """
        re_series = re.match(r'series/(\d*)/$', subpath)
        re_patch = re.match(r'patches/(\d*)/$', subpath)
        re_comments = re.match(r'patches/(\d*)/comments/$', subpath)
        if re_series:
            series_num = re_series.group(1)
            if series_num == '1234':
                return {'patches': self.patches}
        elif re_patch:
            patch_num = int(re_patch.group(1))
            patch = self.patches[patch_num - 1]
            return patch
        elif re_comments:
            patch_num = int(re_comments.group(1))
            patch = self.patches[patch_num - 1]
            return patch.comments
        raise ValueError('Fake Patchwork does not understand: %s' % subpath)

    def test_create_branch(self):
        """Test operation of create_branch()"""
        repo = self.make_git_tree()
        branch = 'first'
        dest_branch = 'first2'
        count = 2
        gitdir = self.gitdir

        # Set up the test git tree. We use branch 'first' which has two commits
        # in it
        series = patchstream.get_metadata_for_list(branch, gitdir, count)
        self.assertEqual(2, len(series.commits))

        patch1 = status.Patch('1')
        patch1.parse_subject('[1/2] %s' % series.commits[0].subject)
        patch1.name = patch1.raw_subject
        patch1.content = 'This is my patch content'
        comment1a = {'content': 'Reviewed-by: %s\n' % self.joe}

        patch1.comments = [comment1a]

        patch2 = status.Patch('2')
        patch2.parse_subject('[2/2] %s' % series.commits[1].subject)
        patch2.name = patch2.raw_subject
        patch2.content = 'Some other patch content'
        comment2a = {
            'content': 'Reviewed-by: %s\nTested-by: %s\n' %
                       (self.mary, self.leb)}
        comment2b = {
            'content': 'Reviewed-by: %s' % self.fred}
        patch2.comments = [comment2a, comment2b]

        # This test works by setting up patches for use by the fake Rest API
        # function _fake_patchwork3(). The fake patch comments above should
        # result in new review tags that are collected and added to the commits
        # created in the destination branch.
        self.patches = [patch1, patch2]
        count = 2

        # Expected output:
        #   1 i2c: I2C things
        #   + Reviewed-by: Joe Bloggs <joe@napierwallies.co.nz>
        #   2 spi: SPI fixes
        #   + Reviewed-by: Fred Bloggs <f.bloggs@napier.net>
        #   + Reviewed-by: Mary Bloggs <mary@napierwallies.co.nz>
        #   + Tested-by: Lord Edmund Blackaddër <weasel@blackadder.org>
        # 4 new responses available in patchwork
        # 4 responses added from patchwork into new branch 'first2'
        # <unittest.result.TestResult run=8 errors=0 failures=0>

        terminal.set_print_test_mode()
        pwork = Patchwork.for_testing(self._fake_patchwork3)
        status.check_patchwork_status(
            series, '1234', branch, dest_branch, False, False, False, pwork,
            repo)
        lines = terminal.get_print_test_lines()
        self.assertEqual(12, len(lines))
        self.assertEqual(
            "4 responses added from patchwork into new branch 'first2'",
            lines[11].text)

        # Check that the destination branch has the new tags
        new_series = patchstream.get_metadata_for_list(dest_branch, gitdir,
                                                       count)
        self.assertEqual(
            {'Reviewed-by': {self.joe}},
            new_series.commits[0].rtags)
        self.assertEqual(
            {'Tested-by': {self.leb},
             'Reviewed-by': {self.fred, self.mary}},
            new_series.commits[1].rtags)

        # Now check the actual test of the first commit message. We expect to
        # see the new tags immediately below the old ones.
        stdout = patchstream.get_list(dest_branch, count=count, git_dir=gitdir)
        lines = iter([line.strip() for line in stdout.splitlines()
                      if '-by:' in line])

        # First patch should have the review tag
        self.assertEqual('Reviewed-by: %s' % self.joe, next(lines))

        # Second patch should have the sign-off then the tested-by and two
        # reviewed-by tags
        self.assertEqual('Signed-off-by: %s' % self.leb, next(lines))
        self.assertEqual('Reviewed-by: %s' % self.fred, next(lines))
        self.assertEqual('Reviewed-by: %s' % self.mary, next(lines))
        self.assertEqual('Tested-by: %s' % self.leb, next(lines))

    def test_parse_snippets(self):
        """Test parsing of review snippets"""
        text = '''Hi Fred,

This is a comment from someone.

Something else

On some recent date, Fred wrote:
> This is why I wrote the patch
> so here it is

Now a comment about the commit message
A little more to say

Even more

> diff --git a/file.c b/file.c
> Some more code
> Code line 2
> Code line 3
> Code line 4
> Code line 5
> Code line 6
> Code line 7
> Code line 8
> Code line 9

And another comment

> @@ -153,8 +143,13 @@ def check_patch(fname, show_types=False):
>  further down on the file
>  and more code
> +Addition here
> +Another addition here
>  codey
>  more codey

and another thing in same file

> @@ -253,8 +243,13 @@
>  with no function context

one more thing

> diff --git a/tools/patman/main.py b/tools/patman/main.py
> +line of code
now a very long comment in a different file
line2
line3
line4
line5
line6
line7
line8
'''
        pstrm = PatchStream.process_text(text, True)
        self.assertEqual([], pstrm.commit.warn)

        # We expect to the filename and up to 5 lines of code context before
        # each comment. The 'On xxx wrote:' bit should be removed.
        self.assertEqual(
            [['Hi Fred,',
              'This is a comment from someone.',
              'Something else'],
             ['> This is why I wrote the patch',
              '> so here it is',
              'Now a comment about the commit message',
              'A little more to say', 'Even more'],
             ['> File: file.c', '> Code line 5', '> Code line 6',
              '> Code line 7', '> Code line 8', '> Code line 9',
              'And another comment'],
             ['> File: file.c',
              '> Line: 153 / 143: def check_patch(fname, show_types=False):',
              '>  and more code', '> +Addition here', '> +Another addition here',
              '>  codey', '>  more codey', 'and another thing in same file'],
             ['> File: file.c', '> Line: 253 / 243',
              '>  with no function context', 'one more thing'],
             ['> File: tools/patman/main.py', '> +line of code',
              'now a very long comment in a different file',
              'line2', 'line3', 'line4', 'line5', 'line6', 'line7', 'line8']],
            pstrm.snippets)

    def test_review_snippets(self):
        """Test showing of review snippets"""
        def _to_submitter(who):
            m_who = re.match('(.*) <(.*)>', who)
            return {
                'name': m_who.group(1),
                'email': m_who.group(2)
                }

        commit1 = Commit('abcd')
        commit1.subject = 'Subject 1'
        commit2 = Commit('ef12')
        commit2.subject = 'Subject 2'

        patch1 = status.Patch('1')
        patch1.parse_subject('[1/2] Subject 1')
        patch1.name = patch1.raw_subject
        patch1.content = 'This is my patch content'
        comment1a = {'submitter': _to_submitter(self.joe),
                     'content': '''Hi Fred,

On some date Fred wrote:

> diff --git a/file.c b/file.c
> Some code
> and more code

Here is my comment above the above...


Reviewed-by: %s
''' % self.joe}

        patch1.comments = [comment1a]

        patch2 = status.Patch('2')
        patch2.parse_subject('[2/2] Subject 2')
        patch2.name = patch2.raw_subject
        patch2.content = 'Some other patch content'
        comment2a = {
            'content': 'Reviewed-by: %s\nTested-by: %s\n' %
                       (self.mary, self.leb)}
        comment2b = {'submitter': _to_submitter(self.fred),
                     'content': '''Hi Fred,

On some date Fred wrote:

> diff --git a/tools/patman/commit.py b/tools/patman/commit.py
> @@ -41,6 +41,9 @@ class Commit:
>          self.rtags = collections.defaultdict(set)
>          self.warn = []
>
> +    def __str__(self):
> +        return self.subject
> +
>      def add_change(self, version, info):
>          """Add a new change line to the change list for a version.
>
A comment

Reviewed-by: %s
''' % self.fred}
        patch2.comments = [comment2a, comment2b]

        # This test works by setting up commits and patch for use by the fake
        # Rest API function _fake_patchwork2(). It calls various functions in
        # the status module after setting up tags in the commits, checking that
        # things behaves as expected
        self.commits = [commit1, commit2]
        self.patches = [patch1, patch2]

        # Check that the output patches expectations:
        #   1 Subject 1
        #     Reviewed-by: Joe Bloggs <joe@napierwallies.co.nz>
        #   2 Subject 2
        #     Tested-by: Lord Edmund Blackaddër <weasel@blackadder.org>
        #     Reviewed-by: Fred Bloggs <f.bloggs@napier.net>
        #   + Reviewed-by: Mary Bloggs <mary@napierwallies.co.nz>
        # 1 new response available in patchwork

        series = Series()
        series.commits = [commit1, commit2]
        terminal.set_print_test_mode()
        pwork = Patchwork.for_testing(self._fake_patchwork2)
        col = status.check_patchwork_status(
            series, '1234', None, None, False, True, False, pwork)
        lines = iter(terminal.get_print_test_lines())
        col = terminal.Color()
        self.assertEqual(terminal.PrintLine('  1 Subject 1', col.YELLOW),
                         next(lines))
        self.assertEqual(
            terminal.PrintLine('  + Reviewed-by: ', col.GREEN, newline=False),
            next(lines))
        self.assertEqual(terminal.PrintLine(self.joe, col.WHITE), next(lines))

        self.assertEqual(terminal.PrintLine('Review: %s' % self.joe, col.RED),
                         next(lines))
        self.assertEqual(terminal.PrintLine('    Hi Fred,', None), next(lines))
        self.assertEqual(terminal.PrintLine('', None), next(lines))
        self.assertEqual(terminal.PrintLine('    > File: file.c', col.MAGENTA),
                         next(lines))
        self.assertEqual(terminal.PrintLine('    > Some code', col.MAGENTA),
                         next(lines))
        self.assertEqual(terminal.PrintLine('    > and more code', col.MAGENTA),
                         next(lines))
        self.assertEqual(terminal.PrintLine(
            '    Here is my comment above the above...', None), next(lines))
        self.assertEqual(terminal.PrintLine('', None), next(lines))

        self.assertEqual(terminal.PrintLine('  2 Subject 2', col.YELLOW),
                         next(lines))
        self.assertEqual(
            terminal.PrintLine('  + Reviewed-by: ', col.GREEN, newline=False),
            next(lines))
        self.assertEqual(terminal.PrintLine(self.fred, col.WHITE),
                         next(lines))
        self.assertEqual(
            terminal.PrintLine('  + Reviewed-by: ', col.GREEN, newline=False),
            next(lines))
        self.assertEqual(terminal.PrintLine(self.mary, col.WHITE),
                         next(lines))
        self.assertEqual(
            terminal.PrintLine('  + Tested-by: ', col.GREEN, newline=False),
            next(lines))
        self.assertEqual(terminal.PrintLine(self.leb, col.WHITE),
                         next(lines))

        self.assertEqual(terminal.PrintLine('Review: %s' % self.fred, col.RED),
                         next(lines))
        self.assertEqual(terminal.PrintLine('    Hi Fred,', None), next(lines))
        self.assertEqual(terminal.PrintLine('', None), next(lines))
        self.assertEqual(terminal.PrintLine(
            '    > File: tools/patman/commit.py', col.MAGENTA), next(lines))
        self.assertEqual(terminal.PrintLine(
            '    > Line: 41 / 41: class Commit:', col.MAGENTA), next(lines))
        self.assertEqual(terminal.PrintLine(
            '    > +        return self.subject', col.MAGENTA), next(lines))
        self.assertEqual(terminal.PrintLine(
            '    > +', col.MAGENTA), next(lines))
        self.assertEqual(
            terminal.PrintLine('    >      def add_change(self, version, info):',
                               col.MAGENTA),
            next(lines))
        self.assertEqual(terminal.PrintLine(
            '    >          """Add a new change line to the change list for a version.',
            col.MAGENTA), next(lines))
        self.assertEqual(terminal.PrintLine(
            '    >', col.MAGENTA), next(lines))
        self.assertEqual(terminal.PrintLine(
            '    A comment', None), next(lines))
        self.assertEqual(terminal.PrintLine('', None), next(lines))

        self.assertEqual(terminal.PrintLine(
            '4 new responses available in patchwork (use -d to write them to a new branch)',
            None), next(lines))

    def test_insert_tags(self):
        """Test inserting of review tags"""
        msg = '''first line
second line.'''
        tags = [
            'Reviewed-by: Bin Meng <bmeng.cn@gmail.com>',
            'Tested-by: Bin Meng <bmeng.cn@gmail.com>'
            ]
        signoff = 'Signed-off-by: Simon Glass <sjg@chromium.com>'
        tag_str = '\n'.join(tags)

        new_msg = patchstream.insert_tags(msg, tags)
        self.assertEqual(msg + '\n\n' + tag_str, new_msg)

        new_msg = patchstream.insert_tags(msg + '\n', tags)
        self.assertEqual(msg + '\n\n' + tag_str, new_msg)

        msg += '\n\n' + signoff
        new_msg = patchstream.insert_tags(msg, tags)
        self.assertEqual(msg + '\n' + tag_str, new_msg)

    def test_database_setup(self):
        """Check setting up of the series database"""
        cser = cseries.Cseries(self.tmpdir)
        with terminal.capture() as (_, err):
            cser.open_database()
        self.assertEqual(f'Creating new database {self.tmpdir}/.patman.db',
                         err.getvalue().strip())
        res = cser.db.execute("SELECT name FROM series")
        self.assertTrue(res)
        cser.close_database()

    def get_database(self):
        """Open the database and silence the warning output

        Return:
            Cseries: Resulting Cseries object
        """
        cser = cseries.Cseries(self.tmpdir, terminal.COLOR_NEVER)
        with terminal.capture() as _:
            cser.open_database()
        self.cser = cser
        return cser

    def get_cser(self):
        """Set up a git tree and database

        Return:
            Cseries: object
        """
        self.make_git_tree()
        return self.get_database()

    def db_close(self):
        if self.cser and self.cser.db.cur:
            self.cser.close_database()
            return True
        return False

    def db_open(self):
        if self.cser and not self.cser.db.cur:
            self.cser.open_database()

    def run_args(self, *argv, expected_ret=0, pwork=None):
        was_open = self.db_close()
        args = cmdline.parse_args(['-D'] + list(argv), config_fname=False)
        exit_code = control.do_patman(args, self.tmpdir, pwork)
        self.assertEqual(expected_ret, exit_code)
        if was_open:
            self.db_open()

    def test_series_add(self):
        """Test adding a new cseries"""
        cser = self.get_cser()
        self.assertFalse(cser.get_series_dict())

        with terminal.capture() as (out, _):
            cser.add_series('first', 'my description', allow_unmarked=True)
        lines = out.getvalue().strip().splitlines()
        self.assertEqual(
            "Adding series 'first' v1: mark False allow_unmarked True",
            lines[0])
        self.assertEqual("Added series 'first' v1 (2 commits)", lines[1])
        self.assertEqual(2, len(lines))

        slist = cser.get_series_dict()
        self.assertEqual(1, len(slist))
        self.assertEqual('first', slist['first'].name)
        self.assertEqual('my description', slist['first'].desc)

        plist = cser.get_ser_ver_list()
        self.assertEqual(1, len(plist))
        self.assertEqual((1, 1, 1, None, None, None, None), plist[0])

        pclist = cser.get_pcommit_dict()
        self.assertEqual(2, len(pclist))
        self.assertIn(1, pclist)
        self.assertEqual(
            PCOMMIT(1, 0, 'i2c: I2C things', 1, None, None, None, None),
            pclist[1])
        self.assertEqual(
            PCOMMIT(2, 1, 'spi: SPI fixes', 1, None, None, None, None),
            pclist[2])

        self.db_close()

    def test_series_not_checked_out(self):
        """Test adding a new cseries when a different one is checked out"""
        cser = self.get_cser()
        self.assertFalse(cser.get_series_dict())

        with terminal.capture() as (out, _):
            cser.add_series('second', allow_unmarked=True)
        lines = out.getvalue().strip().splitlines()
        self.assertEqual(
            "Adding series 'second' v1: mark False allow_unmarked True",
            lines[0])
        self.assertEqual("Added series 'second' v1 (3 commits)", lines[1])
        self.assertEqual(2, len(lines))

    def test_series_add_manual(self):
        """Test adding a new cseries with a version number"""
        cser = self.get_cser()
        self.assertFalse(cser.get_series_dict())

        repo = pygit2.init_repository(self.gitdir)
        first_target = repo.revparse_single('first')
        repo.branches.local.create('first2', first_target)
        repo.config.set_multivar('branch.first2.remote', '', '.')
        repo.config.set_multivar('branch.first2.merge', '', 'refs/heads/base')

        with terminal.capture() as (out, _):
            cser.add_series('first2', 'description', allow_unmarked=True)
        lines = out.getvalue().splitlines()
        self.assertEqual(
            "Adding series 'first' v2: mark False allow_unmarked True",
            lines[0])
        self.assertEqual("Added series 'first' v2 (2 commits)", lines[1])
        self.assertEqual(2, len(lines))

        slist = cser.get_series_dict()
        self.assertEqual(1, len(slist))
        self.assertEqual('first', slist['first'].name)

        # We should have just one entry, with version 2
        plist = cser.get_ser_ver_list()
        self.assertEqual(1, len(plist))
        self.assertEqual((1, 1, 2, None, None, None, None), plist[0])

    def add_first2(self, checkout):
        """Add a new first2 branch, a copy of first"""
        repo = pygit2.init_repository(self.gitdir)
        first_target = repo.revparse_single('first')
        repo.branches.local.create('first2', first_target)
        repo.config.set_multivar('branch.first2.remote', '', '.')
        repo.config.set_multivar('branch.first2.merge', '', 'refs/heads/base')

        target = repo.lookup_reference('refs/heads/first2')
        repo.checkout(target, strategy=pygit2.GIT_CHECKOUT_FORCE)

    def test_series_add_different(self):
        """Test adding a different vers. of a series from the checked out one"""
        cser = self.get_cser()

        self.add_first2(True)

        # Add first2 initially
        with terminal.capture() as (out, _):
            cser.add_series(None, 'description', allow_unmarked=True)
        lines = out.getvalue().splitlines()
        self.assertEqual(
            "Adding series 'first' v2: mark False allow_unmarked True",
            lines[0])
        self.assertEqual("Added series 'first' v2 (2 commits)", lines[1])
        self.assertEqual(2, len(lines))

        # Now add first: it should be added as a new version
        with terminal.capture() as (out, _):
            cser.add_series('first', 'description', allow_unmarked=True)
        lines = out.getvalue().splitlines()
        self.assertEqual(
            "Adding series 'first' v1: mark False allow_unmarked True",
            lines[0])
        self.assertEqual(
            "Added v1 to existing series 'first' (2 commits)", lines[1])
        self.assertEqual(2, len(lines))

        slist = cser.get_series_dict()
        self.assertEqual(1, len(slist))
        self.assertEqual('first', slist['first'].name)

        # We should have two entries, one of each version
        plist = cser.get_ser_ver_list()
        self.assertEqual(2, len(plist))
        self.assertEqual((1, 1, 2, None, None, None, None), plist[0])
        self.assertEqual((2, 1, 1, None, None, None, None), plist[1])

    def test_series_add_dup(self):
        """Test adding a series twice"""
        cser = self.get_cser()
        with terminal.capture() as (out, _):
            cser.add_series(None, 'description', allow_unmarked=True)

        with terminal.capture() as (out, _):
            cser.add_series(None, 'description', allow_unmarked=True)
        self.assertIn("Series 'first' v1 already exists",
                      out.getvalue().strip())

        self.add_first2(False)

        with terminal.capture() as (out, _):
            cser.add_series(None, 'description', allow_unmarked=True)
        lines = out.getvalue().splitlines()
        self.assertEqual(
            "Added v2 to existing series 'first' (2 commits)", lines[1])

    def test_series_add_dup_reverse(self):
        """Test adding a series twice, v2 then v1"""
        cser = self.get_cser()
        self.add_first2(True)
        with terminal.capture() as (out, _):
            cser.add_series(None, 'description', allow_unmarked=True)
        self.assertIn("Added series 'first' v2", out.getvalue().strip())

        with terminal.capture() as (out, _):
            cser.add_series('first', 'description', allow_unmarked=True)
        self.assertIn("Added v1 to existing series 'first'",
                      out.getvalue().strip())

    def test_series_add_dup_reverse_cmdline(self):
        """Test adding a series twice, v2 then v1"""
        cser = self.get_cser()
        self.add_first2(True)
        with terminal.capture() as (out, _):
            self.run_args('series', 'add', '-M', '-d', 'description',
                          pwork=True)
        self.assertIn("Added series 'first' v2 (2 commits)",
                      out.getvalue().strip())

        with terminal.capture() as (out, _):
            self.run_args('series', '-s', 'first', 'add', '-M',
                          '-d', 'description', pwork=True)
            cser.add_series('first', 'description', allow_unmarked=True)
        self.assertIn("Added v1 to existing series 'first'",
                      out.getvalue().strip())

    def test_series_add_skip_version(self):
        """Test adding a series which is v4 but has no earlier version"""
        cser = self.get_cser()
        with terminal.capture() as (out, _):
            cser.add_series('third4', 'The glorious third series', mark=False,
                            allow_unmarked=True)
        lines = out.getvalue().splitlines()
        self.assertEqual(
            "Adding series 'third' v4: mark False allow_unmarked True", lines[0])
        self.assertEqual("Added series 'third' v4 (4 commits)", lines[1])
        self.assertEqual(2, len(lines))

        sdict = cser.get_series_dict()
        self.assertIn('third', sdict)
        chk = sdict['third']
        self.assertEqual('third', chk['name'])
        self.assertEqual('The glorious third series', chk['desc'])

        svid = cser.get_series_svid(chk['idnum'], 4)
        self.assertEqual(4, len(cser.get_pcommit_dict(svid)))

        # Remove the series and add it again with just two commits
        with terminal.capture():
            cser.remove_series('third4')

        with terminal.capture() as (out, _):
            cser.add_series('third4', 'The glorious third series', mark=False,
                                allow_unmarked=True, end='third4~2')
        lines = out.getvalue().splitlines()
        self.assertEqual(
            "Adding series 'third' v4: mark False allow_unmarked True",
            lines[0])
        self.assertRegex(
            lines[1],
            'Ending before .* main: Change to the main program')
        self.assertEqual("Added series 'third' v4 (2 commits)", lines[2])

        sdict = cser.get_series_dict()
        self.assertIn('third', sdict)
        chk = sdict['third']
        self.assertEqual('third', chk['name'])
        self.assertEqual('The glorious third series', chk['desc'])

        svid = cser.get_series_svid(chk['idnum'], 4)
        self.assertEqual(2, len(cser.get_pcommit_dict(svid)))

    def test_series_add_wrong_version(self):
        """Test adding a series with an incorrect branch name or version

        This updates branch 'first' to have version 2, then tries to add it.
        """
        cser = self.get_cser()
        self.assertFalse(cser.get_series_dict())

        with terminal.capture():
            _, ser, max_vers, _ = cser._prep_series('first')
            cser.update_series('first', ser, max_vers, None, False, add_vers=2)

        with self.assertRaises(ValueError) as exc:
            with terminal.capture():
                cser.add_series('first', 'my description', allow_unmarked=True)
        self.assertEqual(
            "Series name 'first' suggests version 1 but Series-version tag "
            'indicates 2 (see --force-version)', str(exc.exception))

        # Now try again with --force-version which should force version 1
        with terminal.capture() as (out, err):
            cser.add_series('first', 'my description', allow_unmarked=True,
                            force_version=True)
        lines = iter(out.getvalue().splitlines())
        self.assertEqual(
            "Adding series 'first' v1: mark False allow_unmarked True",
            next(lines))
        self.assertEqual('Checking out upstream commit refs/heads/base',
                         next(lines))
        self.assertEqual(
            "Processing 2 commits from branch 'first'", next(lines))
        self.assertRegex(next(lines),
                         '-  .* as .*: i2c: I2C things')
        self.assertRegex(
            next(lines),
            '- deleted version 1  .* as .*: spi: SPI fixes')
        self.assertRegex(next(lines), 'Updating branch first to .*')
        self.assertEqual("Added series 'first' v1 (2 commits)", next(lines))
        try:
            self.assertEqual('extra line', next(lines))
        except StopIteration:
            pass

        # Since this is v1 the Series-version tag should have been removed
        series = patchstream.get_metadata('first', 0, 2, git_dir=self.gitdir)
        self.assertNotIn('version', series)

    def setup_second(self, do_sync=True):
        """Set up the 'second' series synced with the fake patchwork

        Args:
            do_sync (bool): True to sync the series

        Return: tuple:
            Cseries: New Cseries object
            pwork: Patchwork object
        """
        cser = self.get_cser()
        pwork = Patchwork.for_testing(self._fake_patchwork_cser_link)
        pwork.set_project(PROJ_ID, PROJ_LINK_NAME)

        with terminal.capture() as (out, _):
            cser.add_series('first', '', allow_unmarked=True)
            cser.add_series('second', allow_unmarked=True)
            cser.increment('second')
        if do_sync:
            with terminal.capture() as (out, _):
                cser.autolink(pwork, 'second', 2, True)
            with terminal.capture() as (out, _):
                cser.series_sync(pwork, 'second', 2)
            lines = out.getvalue().splitlines()
            self.assertEqual(
                "Updating series 'second' version 2 from link '457'", lines[0])
            self.assertEqual('3 patches and cover letter updated', lines[1])
            self.assertEqual(2, len(lines))

        return cser, pwork

    def test_series_list(self):
        """Test listing cseries"""
        self.setup_second()

        self.db_close()
        args = Namespace(subcmd='list', extra=[])
        with terminal.capture() as (out, _):
            control.do_series(args, test_db=self.tmpdir, pwork=True)
        lines = out.getvalue().splitlines()
        self.assertEqual(5, len(lines))
        self.assertEqual(
            'Name             Description                               Accepted  Versions',
            lines[0])
        self.assertTrue(lines[1].startswith('--'))
        self.assertEqual(
            'first                                                           -/2  1',
            lines[2])
        self.assertEqual(
            'second           Series for my board                            1/3  1 2',
            lines[3])
        self.assertTrue(lines[4].startswith('--'))
        self.db_close()

    def test_do_series_add(self):
        """Add a new cseries"""
        self.make_git_tree()
        args = Namespace(subcmd='add', desc='my-description', series='first',
                         mark=False, allow_unmarked=True, upstream=None,
                         dry_run=False)
        with terminal.capture() as (out, _):
            control.do_series(args, test_db=self.tmpdir, pwork=True)

        cser = self.get_database()
        slist = cser.get_series_dict()
        self.assertEqual(1, len(slist))
        ser = slist.get('first')
        self.assertTrue(ser)
        self.assertEqual('first', ser.name)
        self.assertEqual('my-description', ser.desc)

        self.db_close()
        args.subcmd = 'list'
        with terminal.capture() as (out, _):
            control.do_series(args, test_db=self.tmpdir, pwork=True)
        lines = out.getvalue().splitlines()
        self.assertEqual(4, len(lines))
        self.assertTrue(lines[1].startswith('--'))
        self.assertEqual(
            'first            my-description                                 -/2  1',
            lines[2])

        self.db_close()

    def test_do_series_add_cmdline(self):
        """Add a new cseries using the cmdline"""
        self.make_git_tree()
        with terminal.capture():
            self.run_args('series', '-s', 'first', 'add', '-M',
                          '-d', 'my-description', pwork=True)

        cser = self.get_database()
        slist = cser.get_series_dict()
        self.assertEqual(1, len(slist))
        ser = slist.get('first')
        self.assertTrue(ser)
        self.assertEqual('first', ser.name)
        self.assertEqual('my-description', ser.desc)
        self.db_close()

    def test_do_series_add_auto(self):
        """Add a new cseries without any arguments"""
        self.make_git_tree()

        # Use the 'second' branch, which has a cover letter
        gitutil.checkout('second', self.gitdir, work_tree=self.tmpdir,
                         force=True)
        args = Namespace(subcmd='add', extra=[], series=None, mark=False,
                         allow_unmarked=True, upstream=None, dry_run=False,
                         desc=None)
        with terminal.capture():
            control.do_series(args, test_db=self.tmpdir, pwork=True)

        cser = self.get_database()
        slist = cser.get_series_dict()
        self.assertEqual(1, len(slist))
        ser = slist.get('second')
        self.assertTrue(ser)
        self.assertEqual('second', ser.name)
        self.assertEqual('Series for my board', ser.desc)
        cser.close_database()

    def _check_inc(self, out):
        """Check output from an 'increment' operation

        Args:
            out (StringIO): Text to check
        """
        lines = iter(out.getvalue().splitlines())

        self.assertEqual("Increment 'first' v1: 2 patches", next(lines))
        self.assertRegex(next(lines), 'Checking out upstream commit .*')
        self.assertEqual("Processing 2 commits from branch 'first2'",
                         next(lines))
        self.assertRegex(next(lines), '-  .* as .*: i2c: I2C things')
        self.assertRegex(next(lines), '- added version 2 .* as .*: spi: SPI fixes')
        self.assertRegex(next(lines), 'Updating branch first2 to .*')
        self.assertEqual('Added new branch first2', next(lines))
        return lines

    def test_series_link(self):
        """Test adding a patchwork link to a cseries"""
        cser = self.get_cser()

        repo = pygit2.init_repository(self.gitdir)
        first = repo.lookup_branch('first').peel(pygit2.GIT_OBJ_COMMIT).oid
        base = repo.lookup_branch('base').peel(pygit2.GIT_OBJ_COMMIT).oid

        gitutil.checkout('first', self.gitdir, work_tree=self.tmpdir,
                         force=True)

        with terminal.capture() as (out, _):
            cser.add_series('first', '', allow_unmarked=True)

        with self.assertRaises(ValueError) as exc:
            cser.set_link('first', 2, '1234', True)
        self.assertEqual("Series 'first' does not have a version 2",
                         str(exc.exception))

        self.assertEqual('first', gitutil.get_branch(self.gitdir))
        with terminal.capture() as (out, _):
            cser.increment('first')
        self.assertTrue(repo.lookup_branch('first2'))

        with terminal.capture() as (out, _):
            cser.set_link('first', 2, '2345', True)

        lines = out.getvalue().splitlines()
        self.assertEqual(6, len(lines))
        self.assertEqual('Checking out upstream commit refs/heads/base',
                         lines[0])
        self.assertEqual("Processing 2 commits from branch 'first2'",
                         lines[1])
        self.assertRegex(lines[2], '-  .* as .*: i2c: I2C things')
        self.assertRegex(lines[3],
                         '- added version 2 .* as .*: spi: SPI fixes')
        self.assertRegex(lines[4], 'Updating branch first2 to .*')
        self.assertEqual("Setting link for series 'first' v2 to 2345",
                         lines[5])

        self.assertEqual('2345', cser.get_link('first', 2))

        series = patchstream.get_metadata_for_list('first2', self.gitdir, 2)
        self.assertEqual('2:2345', series.links)

        self.assertEqual('first2', gitutil.get_branch(self.gitdir))

        # Check the original series was left alone
        self.assertEqual(
            first, repo.lookup_branch('first').peel(pygit2.GIT_OBJ_COMMIT).oid)
        count = 2
        series1 = patchstream.get_metadata_for_list('first', self.gitdir, count)
        self.assertFalse('links' in series1)
        self.assertFalse('version' in series1)

        # Check that base is left alone
        self.assertEqual(
            base, repo.lookup_branch('base').peel(pygit2.GIT_OBJ_COMMIT).oid)
        series1 = patchstream.get_metadata_for_list('base', self.gitdir, count)
        self.assertFalse('links' in series1)
        self.assertFalse('version' in series1)

        # Check out second and try to update first
        gitutil.checkout('second', self.gitdir, work_tree=self.tmpdir,
                         force=True)
        with terminal.capture():
            cser.set_link('first', 1, '16', True)

        # Overwrite the link
        with terminal.capture():
            cser.set_link('first', 1, '17', True)

        series2 = patchstream.get_metadata_for_list('first', self.gitdir, count)
        self.assertEqual('1:17', series2.links)

        self.db_close()

    def test_series_link_cmdline(self):
        """Test adding a patchwork link to a cseries using the cmdline"""
        cser = self.get_cser()

        gitutil.checkout('first', self.gitdir, work_tree=self.tmpdir,
                         force=True)

        with terminal.capture() as (out, _):
            cser.add_series('first', '', allow_unmarked=True)

        with terminal.capture() as (out, _):
            self.run_args('series', '-s', 'first', '-V', '4', 'set-link','-u',
                          '1234', expected_ret=1, pwork=True)
        self.assertIn("Series 'first' does not have a version 4",
                      out.getvalue())

        with self.assertRaises(ValueError) as exc:
            cser.get_link('first', 4)
        self.assertEqual("Series 'first' does not have a version 4",
                         str(exc.exception))

        with terminal.capture() as (out, _):
            cser.increment('first')

        with self.assertRaises(ValueError) as exc:
            cser.get_link('first', 4)
        self.assertEqual("Series 'first' does not have a version 4",
                         str(exc.exception))

        with terminal.capture() as (out, _):
            cser.increment('first')
            cser.increment('first')

        with terminal.capture() as (out, _):
            self.run_args('series', '-s', 'first', '-V', '4', 'set-link', '-u',
                      '1234', pwork=True)
        lines = out.getvalue().splitlines()
        self.assertRegex(
            lines[-3],
            "- added version 4 added links '4:1234' .* as .*: spi: SPI fixes")
        self.assertEqual("Setting link for series 'first' v4 to 1234",
                         lines[-1])

        with terminal.capture() as (out, _):
            self.run_args('series', '-s', 'first', '-V', '4', 'get-link',
                          pwork=True)
        self.assertIn('1234', out.getvalue())

        series = patchstream.get_metadata_for_list('first4', self.gitdir, 1)
        self.assertEqual('4:1234', series.links)

        with terminal.capture() as (out, _):
            self.run_args('series', '-s', 'first', '-V', '5', 'get-link',
                          expected_ret=1, pwork=True)

        self.assertIn("Series 'first' does not have a version 5",
                      out.getvalue())

        # Checkout 'first' and try to get the link from 'first4'
        gitutil.checkout('first', self.gitdir, work_tree=self.tmpdir,
                         force=True)

        with terminal.capture() as (out, _):
            self.run_args('series', '-s', 'first4', 'get-link', pwork=True)
        self.assertIn('1234', out.getvalue())

        # This should get the link for 'first'
        with terminal.capture() as (out, _):
            self.run_args('series', 'get-link', pwork=True)
        self.assertIn('None', out.getvalue())

        # Checkout 'first4' again; this should get the link for 'first4'
        gitutil.checkout('first4', self.gitdir, work_tree=self.tmpdir,
                         force=True)

        with terminal.capture() as (out, _):
            self.run_args('series', 'get-link', pwork=True)
        self.assertIn('1234', out.getvalue())

        self.db_close()

    def _fake_patchwork_cser_link(self, subpath):
        """Fake Patchwork server for the function below

        This handles accessing a series, providing a list consisting of a
        single patch

        Args:
            subpath (str): URL subpath to use
        """
        if subpath == 'projects/':
            return [
                {'id':PROJ_ID, 'name': 'U-Boot', 'link_name': 'uboot'},
                {'id':9, 'name': 'other', 'link_name': 'other'}]
        re_series = re.match(r'series/\?project=(\d+)&q=.*$', subpath)
        if re_series:
            series_num = re_series.group(1)
            result = [
                {'id': 56, 'name': 'contains first name', 'version': 1},
                {'id': 43, 'name': 'has first in it', 'version': 1},
                {'id': 1234, 'name': 'first series', 'version': 1},
                {'id': 456, 'name': 'Series for my board', 'version': 1},
                {'id': 457, 'name': 'Series for my board', 'version': 2},
            ]
            if self.autolink_extra:
                result += [self.autolink_extra]
            return result;
        return self._fake_patchwork_cser(subpath)

    def test_series_link_auto_version(self):
        """Test finding the patchwork link for a cseries automatically"""
        cser = self.get_cser()

        with terminal.capture() as (out, _):
            cser.add_series('second', allow_unmarked=True)

        # Make sure that the link is there
        count = 3
        series = patchstream.get_metadata('second', 0, count,
                                          git_dir=self.gitdir)
        self.assertEqual('183237', series.links)

        # Set link with detected version
        with terminal.capture() as (out, _):
            cser.set_link('second', None, '456', True)
        self.assertEqual(
            "Setting link for series 'second' v1 to 456",
            out.getvalue().splitlines()[-1])

        # Make sure that the link was set
        series = patchstream.get_metadata('second', 0, count,
                                          git_dir=self.gitdir)
        self.assertEqual('1:456', series.links)

        with terminal.capture():
            cser.increment('second')

        # Make sure that the new series gets the same link
        series = patchstream.get_metadata('second2', 0, 3,
                                          git_dir=self.gitdir)

        pwork = Patchwork.for_testing(self._fake_patchwork_cser_link)
        pwork.set_project(PROJ_ID, PROJ_LINK_NAME)
        self.assertFalse(cser.get_project())
        cser.set_project(pwork, 'U-Boot', quiet=True)

        self.assertEqual((456, None, 'second', 1, 'Series for my board'),
                         cser.search_link(pwork, 'second', 1))

        with terminal.capture():
            cser.increment('second')

        self.assertEqual((457, None, 'second', 2, 'Series for my board'),
                         cser.search_link(pwork, 'second', 2))

    def test_series_link_auto_name(self):
        """Test finding the patchwork link for a cseries with auto name"""
        cser = self.get_cser()

        with terminal.capture() as (out, _):
            cser.add_series('first', '', allow_unmarked=True)

        # Set link with detected name
        with self.assertRaises(ValueError) as exc:
            cser.set_link(None, 2, '2345', True)
        self.assertEqual(
            "Series 'first' does not have a version 2", str(exc.exception))

        with terminal.capture():
            cser.increment('first')

        with terminal.capture() as (out, _):
            cser.set_link(None, 2, '2345', True)
        self.assertEqual(
                "Setting link for series 'first' v2 to 2345",
                out.getvalue().splitlines()[-1])

        plist = cser.get_ser_ver_list()
        self.assertEqual(2, len(plist))
        self.assertEqual((1, 1, 1, None, None, None, None), plist[0])
        self.assertEqual((2, 1, 2, '2345', None, None, None), plist[1])

    def test_series_link_auto_name_version(self):
        """Test finding patchwork link for a cseries with auto name + version"""
        cser = self.get_cser()

        with terminal.capture() as (out, _):
            cser.add_series('first', '', allow_unmarked=True)

        # Set link with detected name and version
        with terminal.capture() as (out, _):
            cser.set_link(None, None, '1234', True)
        self.assertEqual(
                "Setting link for series 'first' v1 to 1234",
                out.getvalue().splitlines()[-1])

        with terminal.capture():
            cser.increment('first')

        with terminal.capture() as (out, _):
            cser.set_link(None, None, '2345', True)
        self.assertEqual(
                "Setting link for series 'first' v2 to 2345",
                out.getvalue().splitlines()[-1])

        plist = cser.get_ser_ver_list()
        self.assertEqual(2, len(plist))
        self.assertEqual((1, 1, 1, '1234', None, None, None), plist[0])
        self.assertEqual((2, 1, 2, '2345', None, None, None), plist[1])

    def test_series_link_missing(self):
        """Test finding patchwork link for a cseries but it is missing"""
        cser = self.get_cser()

        with terminal.capture() as (out, _):
            cser.add_series('second', allow_unmarked=True)

        with terminal.capture():
            cser.increment('second')
            cser.increment('second')

        pwork = Patchwork.for_testing(self._fake_patchwork_cser_link)
        pwork.set_project(PROJ_ID, PROJ_LINK_NAME)
        self.assertFalse(cser.get_project())
        cser.set_project(pwork, 'U-Boot', quiet=True)

        self.assertEqual((456, None, 'second', 1, 'Series for my board'),
                         cser.search_link(pwork, 'second', 1))
        self.assertEqual((457, None, 'second', 2, 'Series for my board'),
                         cser.search_link(pwork, 'second', 2))
        res = cser.search_link(pwork, 'second', 3)
        self.assertEqual(
            (None,
             [{'id': 456, 'name': 'Series for my board', 'version': 1},
              {'id': 457, 'name': 'Series for my board', 'version': 2}],
             'second', 3, 'Series for my board'),
             res)

    def check_series_autolink(self):
        """Common code for autolink tests"""
        cser = self.get_cser()

        pwork = Patchwork.for_testing(self._fake_patchwork_cser_link)
        pwork.set_project(PROJ_ID, PROJ_LINK_NAME)
        self.assertFalse(cser.get_project())
        cser.set_project(pwork, 'U-Boot', quiet=True)

        with terminal.capture():
            cser.add_series('first', '', allow_unmarked=True)
            cser.add_series('second', allow_unmarked=True)

        yield cser, pwork

        self.db_open()

        plist = cser.get_ser_ver_list()
        self.assertEqual(2, len(plist))
        self.assertEqual((1, 1, 1, None, None, None, None), plist[0])
        self.assertEqual((2, 2, 1, '456', None, None, None), plist[1])
        yield cser

    def test_series_autolink(self):
        """Test linking a cseries to its patchwork series by description"""
        cor = self.check_series_autolink()
        cser, pwork = next(cor)

        with self.assertRaises(ValueError) as exc:
            cser.autolink(pwork, 'first', None, True)
        self.assertIn("Series 'first' has an empty description",
                      str(exc.exception))

        with terminal.capture() as (out, _):
            cser.autolink(pwork, 'second', None, True)
        self.assertEqual(
                "Setting link for series 'second' v1 to 456",
                out.getvalue().splitlines()[-1])

        cser = next(cor)
        cor.close()

    def test_series_autolink_cmdline(self):
        """Test linking to patchwork series by description on cmdline"""
        cor = self.check_series_autolink()
        _, pwork = next(cor)
        self.db_close()

        with terminal.capture() as (out, _):
            self.run_args('series', '-s', 'second', 'autolink', '-u',
                          pwork=pwork)
        self.assertEqual(
                "Setting link for series 'second' v1 to 456",
                out.getvalue().splitlines()[-1])

        next(cor)
        cor.close()

    def _autolink_setup(self):
        """Set things up for autolink tests

        Return: tuple:
            Cseries object
            Patchwork object
        """
        cser = self.get_cser()

        pwork = Patchwork.for_testing(self._fake_patchwork_cser_link)
        pwork.set_project(PROJ_ID, PROJ_LINK_NAME)
        self.assertFalse(cser.get_project())
        cser.set_project(pwork, 'U-Boot', quiet=True)

        with terminal.capture():
            cser.add_series('first', 'first series', allow_unmarked=True)
            cser.add_series('second', allow_unmarked=True)
            cser.increment('first')
        return cser, pwork

    def test_series_autolink_all(self):
        """Test linking all cseries to their patchwork series by description"""
        cser, pwork = self._autolink_setup()
        with terminal.capture() as (out, _):
            summary = cser.autolink_all(pwork, update_commit=True,
                                        link_all_versions=True,
                                        replace_existing=False, dry_run=True,
                                        show_summary=False)
        self.assertEqual(3, len(summary))
        items = iter(summary.values())
        linked = next(items)
        self.assertEqual(
            ('first', 1, None, 'first series', 'linked:1234'), linked)
        self.assertEqual(
            ('first', 2, None, 'first series', 'not found'), next(items))
        self.assertEqual(
            ('second', 1, '183237', 'Series for my board', 'already:183237'),
            next(items))
        self.assertEqual('Dry run completed', out.getvalue().splitlines()[-1])

        # A second dry run should do exactly the same thing
        with terminal.capture() as (out2, _):
            summary2 = cser.autolink_all(pwork, update_commit=True,
                                         link_all_versions=True,
                                         replace_existing=False, dry_run=True,
                                         show_summary=False)
        self.assertEqual(out.getvalue(), out2.getvalue())
        self.assertEqual(summary, summary2)

        # Now do it for real
        with terminal.capture():
            summary = cser.autolink_all(pwork, update_commit=True,
                                        link_all_versions=True,
                                        replace_existing=False, dry_run=False,
                                        show_summary=False)

        # Check the link was updated
        pdict = cser.get_ser_ver_dict()
        svid = list(summary)[0]
        self.assertEqual('1234', pdict[svid].link)

        series = patchstream.get_metadata_for_list('first', self.gitdir, 2)
        self.assertEqual('1:1234', series.links)

    def test_series_autolink_latest(self):
        """Test linking the lastest versions"""
        cser, pwork = self._autolink_setup()
        with terminal.capture():
            summary = cser.autolink_all(pwork, update_commit=True,
                                        link_all_versions=False,
                                        replace_existing=False, dry_run=False,
                                        show_summary=False)
        self.assertEqual(2, len(summary))
        items = iter(summary.values())
        self.assertEqual(
            ('first', 2, None, 'first series', 'not found'), next(items))
        self.assertEqual(
            ('second', 1, '183237', 'Series for my board', 'already:183237'),
            next(items))

    def test_series_autolink_no_update(self):
        """Test linking the lastest versions without updating commits"""
        cser, pwork = self._autolink_setup()
        with terminal.capture():
            cser.autolink_all(pwork, update_commit=False,
                              link_all_versions=True, replace_existing=False,
                              dry_run=False,
                              show_summary=False)

        series = patchstream.get_metadata_for_list('first', self.gitdir, 2)
        self.assertNotIn('links', series)

    def test_series_autolink_replace(self):
        """Test linking the lastest versions without updating commits"""
        cser, pwork = self._autolink_setup()
        with terminal.capture():
            summary = cser.autolink_all(pwork, update_commit=True,
                                        link_all_versions=True,
                                        replace_existing=True, dry_run=False,
                                        show_summary=False)
        self.assertEqual(3, len(summary))
        items = iter(summary.values())
        linked = next(items)
        self.assertEqual(
            ('first', 1, None, 'first series', 'linked:1234'), linked)
        self.assertEqual(
            ('first', 2, None, 'first series', 'not found'), next(items))
        self.assertEqual(
            ('second', 1, '183237', 'Series for my board', 'linked:456'),
            next(items))

    def test_series_autolink_cmdline(self):
        """Test command-line operation

        This just uses mocks for now since we can rely on the direct tests for
        the actual operation.
        """
        cser, pwork = self._autolink_setup()
        with (mock.patch.object(cseries.Cseries, 'autolink_all',
                                return_value=None) as method):
            self.run_args('series', 'autolink-all', pwork=True)
        method.assert_called_once_with(True, update_commit=False,
                                       link_all_versions=False,
                                       replace_existing=False, dry_run=False,
                                       show_summary=True)

        with (mock.patch.object(cseries.Cseries, 'autolink_all',
                                return_value=None) as method):
            self.run_args('series', 'autolink-all', '-a', pwork=True)
        method.assert_called_once_with(True, update_commit=False,
                                       link_all_versions=True,
                                       replace_existing=False, dry_run=False,
                                       show_summary=True)

        with (mock.patch.object(cseries.Cseries, 'autolink_all',
                                return_value=None) as method):
            self.run_args('series', 'autolink-all', '-a', '-r', pwork=True)
        method.assert_called_once_with(True, update_commit=False,
                                       link_all_versions=True,
                                       replace_existing=True, dry_run=False,
                                       show_summary=True)

        with (mock.patch.object(cseries.Cseries, 'autolink_all',
                                return_value=None) as method):
            self.run_args('series', '-n', 'autolink-all', '-r', pwork=True)
        method.assert_called_once_with(True, update_commit=False,
                                       link_all_versions=False,
                                       replace_existing=True, dry_run=True,
                                       show_summary=True)

        with (mock.patch.object(cseries.Cseries, 'autolink_all',
                                return_value=None) as method):
            self.run_args('series', 'autolink-all', '-u', pwork=True)
        method.assert_called_once_with(True, update_commit=True,
                                       link_all_versions=False,
                                       replace_existing=False, dry_run=False,
                                       show_summary=True)

        # Now do a real one to check the patchwork handling and output
        with terminal.capture() as (out, _):
            self.run_args('series', 'autolink-all', '-a', pwork=pwork)
        lines = iter(out.getvalue().splitlines())
        self.assertEqual(
            '1 series linked, 1 already linked, 1 not found (2 requests)',
             next(lines))
        self.assertEqual('', next(lines))
        self.assertEqual(
            'Name             Version  Description                               Result',
            next(lines))
        self.assertTrue(next(lines).startswith('--'))
        self.assertEqual(
            'first                  1  first series                              linked:1234',
            next(lines))
        self.assertEqual(
            'first                  2  first series                              not found',
            next(lines))
        self.assertEqual(
            'second                 1  Series for my board                       already:183237',
            next(lines))
        self.assertTrue(next(lines).startswith('--'))
        self.assertFinished(lines)

    def check_series_archive(self):
        """Coroutine to run the archive test"""
        cser = self.get_cser()
        with terminal.capture():
            cser.add_series('first', '', allow_unmarked=True)

        # Check the series is visible in the list
        slist = cser.get_series_dict()
        self.assertEqual(1, len(slist))
        self.assertEqual('first', slist['first'].name)

        # Archive it and make sure it is invisible
        yield cser
        # cser.set_archived('first', True)
        slist = cser.get_series_dict()
        self.assertFalse(slist)

        # ...unless we include archived items
        slist = cser.get_series_dict(include_archived=True)
        self.assertEqual(1, len(slist))
        self.assertEqual('first', slist['first'].name)

        # or we unarchive it
        yield cser
        # cser.set_archived('first', False)
        slist = cser.get_series_dict()
        self.assertEqual(1, len(slist))
        self.db_close()
        yield cser

    def test_series_archive(self):
        """Test marking a series as archived"""
        cor = self.check_series_archive()
        cser = next(cor)

        # Archive it and make sure it is invisible
        cser.set_archived('first', True)
        cser = next(cor)
        cser.set_archived('first', False)
        cser = next(cor)
        cor.close()

    def test_series_archive_cmdline(self):
        """Test marking a series as archived with cmdline"""
        cor = self.check_series_archive()
        cser = next(cor)

        # Archive it and make sure it is invisible
        self.run_args('series', '-s', 'first', 'archive', pwork=True)
        cser = next(cor)
        self.run_args('series', '-s', 'first', 'unarchive', pwork=True)
        cser = next(cor)
        cor.close()

    def check_series_inc(self):
        """Coroutine to run the increment test"""
        cser = self.get_cser()

        gitutil.checkout('first', self.gitdir, work_tree=self.tmpdir,
                         force=True)
        with terminal.capture() as (out, _):
            cser.add_series('first', '', allow_unmarked=True)

        with terminal.capture() as (out, _):
            yield cser
        self._check_inc(out)

        slist = cser.get_series_dict()
        self.assertEqual(1, len(slist))

        plist = cser.get_ser_ver_list()
        self.assertEqual(2, len(plist))
        self.assertEqual((1, 1, 1, None, None, None, None), plist[0])
        self.assertEqual((2, 1, 2, None, None, None, None), plist[1])

        series = patchstream.get_metadata_for_list('first2', self.gitdir, 1)
        self.assertEqual('2', series.version)

        series = patchstream.get_metadata_for_list('first', self.gitdir, 1)
        self.assertNotIn('version', series)

        self.assertEqual('first2', gitutil.get_branch(self.gitdir))
        yield cser

    def test_series_inc(self):
        """Test incrementing the version"""
        cor = self.check_series_inc()
        cser = next(cor)

        cser.increment('first')
        cser = next(cor)

        cor.close()

    def test_series_inc_cmdline(self):
        """Test incrementing the version with cmdline"""
        cor = self.check_series_inc()
        cser = next(cor)

        self.run_args('series', '-s', 'first', 'inc', pwork=True)

        next(cor)
        cor.close()

    def test_series_inc_no_upstream(self):
        """Increment a series which has no upstream branch"""
        cser = self.get_cser()

        gitutil.checkout('first', self.gitdir, work_tree=self.tmpdir,
                         force=True)
        with terminal.capture() as (out, _):
            cser.add_series('first', '', allow_unmarked=True)

        repo = pygit2.init_repository(self.gitdir)
        upstream = repo.lookup_branch('base')
        upstream.delete()
        with terminal.capture() as (out, _):
            cser.increment('first')

        slist = cser.get_series_dict()
        self.assertEqual(1, len(slist))

    def test_series_inc_dryrun(self):
        """Test incrementing the version with cmdline"""
        cser = self.get_cser()

        gitutil.checkout('first', self.gitdir, work_tree=self.tmpdir,
                         force=True)
        with terminal.capture() as (out, _):
            cser.add_series('first', '', allow_unmarked=True)

        with terminal.capture() as (out, _):
            cser.increment('first', dry_run=True)
        lines = self._check_inc(out)
        self.assertEqual('Dry run completed', next(lines))

        # Make sure that nothing was added
        plist = cser.get_ser_ver_list()
        self.assertEqual(1, len(plist))
        self.assertEqual((1, 1, 1, None, None, None, None), plist[0])

        # We should still be on the same branch
        self.assertEqual('first', gitutil.get_branch(self.gitdir))

    def test_series_dec(self):
        """Test decrementing the version"""
        cser = self.get_cser()

        gitutil.checkout('first', self.gitdir, work_tree=self.tmpdir,
                         force=True)
        with terminal.capture() as (out, _):
            cser.add_series('first', '', allow_unmarked=True)

        pclist = cser.get_pcommit_dict()
        self.assertEqual(2, len(pclist))

        # Try decrementing when there is only one version
        with self.assertRaises(ValueError) as exc:
            cser.decrement('first')
        self.assertEqual("Series 'first' only has one version",
                         str(exc.exception))

        # Add a version; now there should be two
        with terminal.capture() as (out, _):
            cser.increment('first')
        svdict = cser.get_ser_ver_dict()
        self.assertEqual(2, len(svdict))

        pclist = cser.get_pcommit_dict()
        self.assertEqual(4, len(pclist))

        # Remove version two, using dry run (i.e. no effect)
        with terminal.capture() as (out, _):
            cser.decrement('first', dry_run=True)
        svdict = cser.get_ser_ver_dict()
        self.assertEqual(2, len(svdict))

        repo = pygit2.init_repository(self.gitdir)
        branch = repo.lookup_branch('first2')
        self.assertTrue(branch)
        branch_oid = branch.peel(pygit2.GIT_OBJ_COMMIT).oid

        pclist = cser.get_pcommit_dict()
        self.assertEqual(4, len(pclist))

        # Now remove version two for real
        with terminal.capture() as (out, _):
            cser.decrement('first')
        lines = out.getvalue().splitlines()
        self.assertEqual(2, len(lines))
        self.assertEqual("Removing series 'first' v2", lines[0])
        self.assertEqual(
            f"Deleted branch 'first2' {str(branch_oid)[:10]}", lines[1])

        svdict = cser.get_ser_ver_dict()
        self.assertEqual(1, len(svdict))

        pclist = cser.get_pcommit_dict()
        self.assertEqual(2, len(pclist))

        branch = repo.lookup_branch('first2')
        self.assertFalse(branch)

        # Removing the only version should not be allowed
        with self.assertRaises(ValueError) as exc:
            cser.decrement('first', dry_run=True)
        self.assertEqual("Series 'first' only has one version",
                         str(exc.exception))

    def test_upstream_add(self):
        cser = self.get_cser()

        cser.add_upstream('us', 'https://one')
        ulist = cser.get_upstream_dict()
        self.assertEqual(1, len(ulist))
        self.assertEqual(('https://one', None), ulist['us'])

        cser.add_upstream('ci', 'git@two')
        ulist = cser.get_upstream_dict()
        self.assertEqual(2, len(ulist))
        self.assertEqual(('https://one', None), ulist['us'])
        self.assertEqual(('git@two', None), ulist['ci'])

        # Try to add a duplicate
        with self.assertRaises(ValueError) as exc:
            cser.add_upstream('ci', 'git@three')
        self.assertEqual("Upstream 'ci' already exists", str(exc.exception))

        with terminal.capture() as (out, err):
            cser.list_upstream()
        lines = out.getvalue().splitlines()
        self.assertEqual(2, len(lines))
        self.assertEqual('us                       https://one', lines[0])
        self.assertEqual('ci                       git@two', lines[1])

        self.db_close()

    def test_upstream_add_cmdline(self):
        with terminal.capture() as (out, err):
            self.run_args('upstream', 'add', 'us', 'https://one')

        with terminal.capture() as (out, err):
            self.run_args('upstream', 'list')
        lines = out.getvalue().splitlines()
        self.assertEqual(1, len(lines))
        self.assertEqual('us                       https://one', lines[0])

        self.db_close()

    def test_upstream_default(self):
        cser = self.get_cser()

        with self.assertRaises(ValueError) as exc:
            cser.set_default_upstream('us')
        self.assertEqual("No such upstream 'us'", str(exc.exception))

        cser.add_upstream('us', 'https://one')
        cser.add_upstream('ci', 'git@two')

        self.assertIsNone(cser.get_default_upstream())

        cser.set_default_upstream('us')
        self.assertEqual('us', cser.get_default_upstream())

        cser.set_default_upstream('us')

        cser.set_default_upstream('ci')
        self.assertEqual('ci', cser.get_default_upstream())

        with terminal.capture() as (out, err):
            cser.list_upstream()
        lines = out.getvalue().splitlines()
        self.assertEqual(2, len(lines))
        self.assertEqual('us                       https://one', lines[0])
        self.assertEqual('ci              default  git@two', lines[1])

        cser.set_default_upstream(None)
        self.assertIsNone(cser.get_default_upstream())

    def test_upstream_default_cmdline(self):
        with terminal.capture() as (out, _):
            self.run_args('upstream', 'default', 'us', expected_ret=1)
        self.assertEqual("patman: ValueError: No such upstream 'us'",
                         out.getvalue().strip().splitlines()[-1])

        self.run_args('upstream', 'add', 'us', 'https://one')
        self.run_args('upstream', 'add', 'ci', 'git@two')

        with terminal.capture() as (out, _):
            self.run_args('upstream', 'default')
        self.assertEqual('unset', out.getvalue().strip())

        self.run_args('upstream', 'default', 'us')
        with terminal.capture() as (out, _):
            self.run_args('upstream', 'default')
        self.assertEqual('us', out.getvalue().strip())

        self.run_args('upstream', 'default', 'ci')
        with terminal.capture() as (out, _):
            self.run_args('upstream', 'default')
        self.assertEqual('ci', out.getvalue().strip())

        with terminal.capture() as (out, _):
            self.run_args('upstream', 'default', '--unset')
        self.assertFalse(out.getvalue().strip())

        with terminal.capture() as (out, _):
            self.run_args('upstream', 'default')
        self.assertEqual('unset', out.getvalue().strip())

    def test_upstream_delete(self):
        cser = self.get_cser()

        with self.assertRaises(ValueError) as exc:
            cser.delete_upstream('us')
        self.assertEqual("No such upstream 'us'", str(exc.exception))

        cser.add_upstream('us', 'https://one')
        cser.add_upstream('ci', 'git@two')

        cser.set_default_upstream('us')
        cser.delete_upstream('us')
        self.assertIsNone(cser.get_default_upstream())

        cser.delete_upstream('ci')
        ulist = cser.get_upstream_dict()
        self.assertFalse(ulist)

    def test_upstream_delete_cmdline(self):
        with terminal.capture() as (out, _):
            self.run_args('upstream', 'delete', 'us', expected_ret=1)
        self.assertEqual("patman: ValueError: No such upstream 'us'",
                         out.getvalue().strip().splitlines()[-1])

        self.run_args('us', 'add', 'us', 'https://one')
        self.run_args('us', 'add', 'ci', 'git@two')

        self.run_args('upstream', 'default', 'us')
        self.run_args('upstream', 'delete', 'us')
        with terminal.capture() as (out, _):
            self.run_args('upstream', 'default', 'us', expected_ret=1)
        self.assertEqual("patman: ValueError: No such upstream 'us'",
                         out.getvalue().strip())

        self.run_args('upstream', 'delete', 'ci')
        with terminal.capture() as (out, _):
            self.run_args('upstream', 'list')
        self.assertFalse(out.getvalue().strip())

    def test_series_add_mark(self):
        """Test marking a cseries with Change-Id fields"""
        cser = self.get_cser()

        with terminal.capture() as (out, _):
            cser.add_series('first', '', mark=True)

        pcdict = cser.get_pcommit_dict()

        series = patchstream.get_metadata('first', 0, 2, git_dir=self.gitdir)
        self.assertEqual(2, len(series.commits))
        self.assertIn(1, pcdict)
        self.assertEqual(1, pcdict[1].id)
        self.assertEqual('i2c: I2C things', pcdict[1].subject)
        self.assertEqual(1, pcdict[1].svid)
        self.assertEqual(series.commits[0].change_id, pcdict[1].change_id)

        self.assertIn(2, pcdict)
        self.assertEqual(2, pcdict[2].id)
        self.assertEqual('spi: SPI fixes', pcdict[2].subject)
        self.assertEqual(1, pcdict[2].svid)
        self.assertEqual(series.commits[1].change_id, pcdict[2].change_id)

    def test_series_add_mark_fail(self):
        """Test marking a cseries when the tree is dirty"""
        cser = self.get_cser()

        # TODO: check that it requires a clean tree
        tools.write_file(os.path.join(self.tmpdir, 'fname'), b'123')
        with terminal.capture() as (out, _):
            cser.add_series('first', '', mark=True)

        tools.write_file(os.path.join(self.tmpdir, 'i2c.c'), b'123')
        with self.assertRaises(ValueError) as exc:
            with terminal.capture() as (out, _):
                cser.add_series('first', '', mark=True)
        self.assertEqual(
            "Modified files exist: use 'git status' to check: [' M i2c.c']",
            str(exc.exception))

    def test_series_add_mark_dry_run(self):
        """Test marking a cseries with Change-Id fields"""
        cser = self.get_cser()

        with terminal.capture() as (out, _):
            cser.add_series('first', '', mark=True, dry_run=True)
        lines = iter(out.getvalue().splitlines())
        self.assertEqual(
            "Adding series 'first' v1: mark True allow_unmarked False",
            next(lines))
        self.assertEqual('Checking out upstream commit refs/heads/base',
                         next(lines))
        self.assertEqual("Processing 2 commits from branch 'first'",
                         next(lines))
        self.assertRegex(next(lines), r'- marked .* as .*: i2c: I2C things')
        self.assertRegex(next(lines), '- marked .* as .*: spi: SPI fixes')
        self.assertRegex(next(lines), 'Updating branch first to .*')
        self.assertEqual("Added series 'first' v1 (2 commits)",
                         next(lines))
        self.assertEqual('Dry run completed', next(lines))

        # Doing another dry run should produce the same result
        with terminal.capture() as (out2, _):
            cser.add_series('first', '', mark=True, dry_run=True)
        self.assertEqual(out.getvalue(), out2.getvalue())

        tools.write_file(os.path.join(self.tmpdir, 'i2c.c'), b'123')
        with terminal.capture() as (out, _):
            with self.assertRaises(ValueError) as exc:
                cser.add_series('first', '', mark=True, dry_run=True)
        self.assertEqual(
            "Modified files exist: use 'git status' to check: [' M i2c.c']",
            str(exc.exception))

        pcdict = cser.get_pcommit_dict()
        self.assertFalse(pcdict)

    def test_series_add_mark_cmdline(self):
        """Test marking a cseries with Change-Id fields using the command line"""
        cser = self.get_cser()

        with terminal.capture() as (out, _):
            self.run_args('series', '-s', 'first', 'add', '-m',
                          '-d', 'my-description', pwork=True)

        pcdict = cser.get_pcommit_dict()
        self.assertTrue(pcdict[1].change_id)
        self.assertTrue(pcdict[2].change_id)

    def test_series_add_unmarked_cmdline(self):
        """Test adding an unmarked cseries using the command line"""
        cser = self.get_cser()

        with terminal.capture() as (out, _):
            self.run_args('series', '-s', 'first', 'add', '-M',
                          '-d', 'my-description', pwork=True)

        pcdict = cser.get_pcommit_dict()
        self.assertFalse(pcdict[1].change_id)
        self.assertFalse(pcdict[2].change_id)

    def test_series_add_unmarked_bad_cmdline(self):
        """Test failure to add an unmarked cseries using a bad command line"""
        cser = self.get_cser()

        with terminal.capture() as (out, _):
            self.run_args('series', '-s', 'first', 'add',
                          '-d', 'my-description', expected_ret=1, pwork=True)
        last_line = out.getvalue().splitlines()[-2]
        self.assertEqual(
            'patman: ValueError: 2 commit(s) are unmarked; please use -m or -M',
            last_line)

    def check_series_unmark(self):
        """Test unmarking a cseries, i.e. removing Change-Id fields"""
        cser = self.get_cser()

        yield cser

        with terminal.capture() as (out, _):
            yield cser

        with terminal.capture() as (out, _):
            yield cser

        lines = iter(out.getvalue().splitlines())
        self.assertEqual(
            "Unmarking series 'first': allow_unmarked False",
            next(lines))
        self.assertEqual('Checking out upstream commit refs/heads/base',
                         next(lines))
        self.assertEqual("Processing 2 commits from branch 'first'",
                         next(lines))
        self.assertRegex(next(lines), '- unmarked .* as .*: i2c: I2C things')
        self.assertRegex(next(lines), '- unmarked .* as .*: spi: SPI fixes')
        self.assertRegex(next(lines), 'Updating branch first to .*')
        self.assertEqual('Dry run completed', next(lines))
        yield None

    def test_series_unmark(self):
        """Test unmarking a cseries, i.e. removing Change-Id fields"""
        cor = self.check_series_unmark()
        cser = next(cor)

        # check the allow_unmarked flag
        with terminal.capture() as (out, _):
            with self.assertRaises(ValueError) as exc:
                cser.unmark_series('first', dry_run=True)
        self.assertEqual('Unmarked commits 2/2', str(exc.exception))

        cser = next(cor)
        cser.add_series('first', '', mark=True)

        cser = next(cor)
        cser.unmark_series('first', dry_run=True)

        self.assertFalse(next(cor))

    def test_series_unmark_cmdline(self):
        """Test the unmark command"""
        cor = self.check_series_unmark()
        next(cor)

        # check the allow_unmarked flag
        with terminal.capture() as (out, _):
            self.run_args('series', 'unmark', expected_ret=1, pwork=True)
        self.assertIn('Unmarked commits 2/2', out.getvalue())

        next(cor)
        self.run_args('series', '-s', 'first', 'add',  '-d', '', '--mark',
                      pwork=True)

        next(cor)
        self.run_args('series', '-s', 'first', '-n', 'unmark', pwork=True)

        self.assertFalse(next(cor))

    def test_series_remove(self):
        """Test removing a series"""
        cser = self.get_cser()

        with self.assertRaises(ValueError) as exc:
            cser.remove_series('first')
        self.assertEqual("No such series 'first'", str(exc.exception))

        with terminal.capture() as (out, _):
            cser.add_series('first', '', mark=True)
        self.assertTrue(cser.get_series_dict())
        pclist = cser.get_pcommit_dict()
        self.assertEqual(2, len(pclist))

        with terminal.capture() as (out, _):
            cser.remove_series('first')
        self.assertEqual("Removed series 'first'", out.getvalue().strip())
        self.assertFalse(cser.get_series_dict())

        pclist = cser.get_pcommit_dict()
        self.assertFalse(len(pclist))

    def test_series_remove_cmdline(self):
        """Test removing a series using the command line"""
        cser = self.get_cser()

        with terminal.capture() as (out, _):
            self.run_args('series', '-s', 'first', 'remove', expected_ret=1,
                          pwork=True)
        self.assertEqual("patman: ValueError: No such series 'first'",
                         out.getvalue().strip())

        with terminal.capture() as (out, _):
            cser.add_series('first', '', mark=True)
        self.assertTrue(cser.get_series_dict())

        with terminal.capture() as (out, _):
            cser.remove_series('first')
        self.assertEqual("Removed series 'first'", out.getvalue().strip())
        self.assertFalse(cser.get_series_dict())

    def check_series_remove_multiple(self):
        """Check for removing a series with more than one version"""
        cser = self.get_cser()

        self.add_first2(True)

        with terminal.capture() as (out, _):
            cser.add_series(None, '', mark=True)
            cser.add_series('first', '', mark=True)
        self.assertTrue(cser.get_series_dict())
        pclist = cser.get_pcommit_dict()
        self.assertEqual(4, len(pclist))

        # Do a dry-run removal
        with terminal.capture() as (out, _):
            yield cser
        self.assertEqual("Removed version 1 from series 'first'\n"
                         'Dry run completed', out.getvalue().strip())
        self.assertEqual({'first'}, cser.get_series_dict().keys())

        plist = cser.get_ser_ver_list()
        self.assertEqual(2, len(plist))
        self.assertEqual((1, 1, 2, None, None, None, None), plist[0])
        self.assertEqual((2, 1, 1, None, None, None, None), plist[1])

        # Now remove for real
        with terminal.capture() as (out, _):
            yield cser
        self.assertEqual("Removed version 1 from series 'first'",
                         out.getvalue().strip())
        self.assertEqual({'first'}, cser.get_series_dict().keys())
        plist = cser.get_ser_ver_list()
        self.assertEqual(1, len(plist))
        pclist = cser.get_pcommit_dict()
        self.assertEqual(2, len(pclist))

        yield cser
        self.assertEqual({'first'}, cser.get_series_dict().keys())

        plist = cser.get_ser_ver_list()
        self.assertEqual(1, len(plist))
        self.assertEqual((1, 1, 2, None, None, None, None), plist[0])

        with terminal.capture() as (out, _):
            yield cser
        self.assertEqual("Removed series 'first'\nDry run completed",
                         out.getvalue().strip())
        self.assertTrue(cser.get_series_dict())
        self.assertTrue(cser.get_ser_ver_list())

        with terminal.capture() as (out, _):
            yield cser
        self.assertEqual("Removed series 'first'", out.getvalue().strip())
        self.assertFalse(cser.get_series_dict())
        self.assertFalse(cser.get_ser_ver_list())
        yield cser

    def test_series_remove_multiple(self):
        """Test removing a series with more than one version"""
        cor = self.check_series_remove_multiple()
        cser = next(cor)

        cser.remove_version('first', 1, dry_run=True)
        cser = next(cor)

        cser.remove_version('first', 1)
        cser = next(cor)

        with self.assertRaises(ValueError) as exc:
            cser.remove_version('first', 2, dry_run=True)
        self.assertEqual(
            "Series 'first' only has one version: remove the series",
            str(exc.exception))
        cser = next(cor)

        cser.remove_series('first', dry_run=True)
        cser = next(cor)

        cser.remove_series('first')
        cser = next(cor)

        cor.close()

    def test_series_remove_multiple_cmdline(self):
        """Test removing a series with more than one version on cmdline"""
        cor = self.check_series_remove_multiple()
        cser = next(cor)

        self.run_args('series', '-n', '-s', 'first', '-V', '1',
                      'remove-version', pwork=True)
        cser = next(cor)

        self.run_args('series', '-s', 'first', '-V', '1', 'remove-version',
                      pwork=True)
        cser = next(cor)

        with terminal.capture() as (out, _):
            self.run_args('series', '-n', '-s', 'first', '-V', '2',
                          'remove-version', expected_ret=1, pwork=True)
        self.assertIn(
            "Series 'first' only has one version: remove the series",
            out.getvalue().strip())
        cser = next(cor)

        self.run_args('series', '-n', '-s', 'first', 'remove', pwork=True)
        cser = next(cor)

        self.run_args('series', '-s', 'first', 'remove', pwork=True)

        cor.close()

    def _fake_patchwork_cser(self, subpath):
        """Fake Patchwork server for the function below

        This handles accessing a series, providing a list consisting of three
        patches, a cover letter and some comments. It also allows three patches
        to be queried.

        Args:
            subpath (str): URL subpath to use
        """
        if subpath == 'projects/':
            return [
                {'id':PROJ_ID, 'name': 'U-Boot', 'link_name': PROJ_LINK_NAME},
                {'id':9, 'name': 'other'}]
        if subpath.startswith('series/'):
            return {
                'patches': [
                    {'id': '10', 'name': '[PATCH,1/3] video: Some video improvements',
                     'content': ''},
                    {'id': '11', 'name': '[PATCH,2/3] serial: Add a serial driver',
                     'content': ''},
                    {'id': '12', 'name': '[PATCH,3/3] bootm: Make it boot',
                     'content': ''},
                ],
                'cover_letter': {
                    'id': 39,
                    'name': 'The name of the cover letter',
                }
            }
        m_pc = re.search(r'patches/(\d*)/comments/', subpath)
        patch_id = m_pc.group(1) if m_pc else ''
        if patch_id:
            if patch_id == '10':
                return [
                    {'id': 1, 'content': ''},
                    {'id': 2,
                     'content': '''On some date Mary Smith <msmith@wibble.com>> wrote:
> This was my original patch
> which is being quoted

I like the approach here and I would love to see more of it.

Reviewed-by: Fred Bloggs <fred@bloggs.com>
''',
                     'submitter': {
                         'name': 'Fred Bloggs',
                         'email': 'fred@bloggs.com',
                         }
                    },
                ]
            if patch_id == '11':
                return []
            if patch_id == '12':
                return [
                    {'id': 4, 'content': ''},
                    {'id': 5, 'content': ''},
                    {'id': 6, 'content': ''},
                ]
            raise ValueError(
                f'Fake Patchwork does not understand patch_id {patch_id} '
                f'type {type(patch_id)}: {subpath}')

        m_cover_id = re.search(r'covers/(\d*)/comments/', subpath)
        cover_id = m_cover_id.group(1) if m_cover_id else ''
        if cover_id:
            if cover_id == '39':
                return [
                    { 'content': 'some comment',
                      'submitter': {
                              'name': 'A user',
                              'email': 'user@user.com',
                          },
                      'date': 'Sun 13 Apr 14:06:02 MDT 2025',
                     },
                    { 'content': 'another comment',
                      'submitter': {
                              'name': 'Ghenkis Kham',
                              'email': 'gk@eurasia.gov',
                          },
                      'date': 'Sun 13 Apr 13:06:02 MDT 2025',
                     },
                ]
            raise ValueError(f'Fake Patchwork unknown cover_id: {cover_id}')

        m_pat = re.search(r'patches/(\d*)/', subpath)
        patch_id = m_pat.group(1) if m_pat else ''
        if subpath.startswith('patches/'):
            if patch_id == '10':
                return {'state': 'accepted',
                        'content': 'Reviewed-by: Fred Bloggs <fred@bloggs.com>'}
            if patch_id == '11':
                return {'state': 'changes-requested', 'content': ''}
            if patch_id == '12':
                return {'state': 'rejected',
                        'content': "I don't like this at all, sorry"}
            raise ValueError(f'Fake Patchwork unknown patch_id: {patch_id}')
        raise ValueError(f'Fake Patchwork does not understand: {subpath}')

    def test_patchwork_set_project(self):
        """Test setting the project ID"""
        cser = self.get_cser()
        pwork = Patchwork.for_testing(self._fake_patchwork_cser)
        with terminal.capture() as (out, _):
            cser.set_project(pwork, 'U-Boot')
        self.assertEqual(
            f"Project 'U-Boot' patchwork-ID {PROJ_ID} link-name uboot",
            out.getvalue().strip())

    def test_patchwork_get_project(self):
        """Test setting the project ID"""
        cser = self.get_cser()
        pwork = Patchwork.for_testing(self._fake_patchwork_cser)
        self.assertFalse(cser.get_project())
        with terminal.capture() as (out, _):
            cser.set_project(pwork, 'U-Boot')
        self.assertEqual(
            f"Project 'U-Boot' patchwork-ID {PROJ_ID} link-name uboot",
            out.getvalue().strip())

        name, pwid, link_name = cser.get_project()
        self.assertEqual('U-Boot', name)
        self.assertEqual(PROJ_ID, pwid)
        self.assertEqual('uboot', link_name)

    def test_patchwork_get_project_cmdline(self):
        """Test setting the project ID"""
        cser = self.get_cser()

        self.assertFalse(cser.get_project())

        pwork = Patchwork.for_testing(self._fake_patchwork_cser)
        with terminal.capture() as (out, _):
            self.run_args('-P', 'https://url', 'patchwork', 'set-project',
                          'U-Boot', pwork=pwork)
        self.assertEqual(
            f"Project 'U-Boot' patchwork-ID {PROJ_ID} link-name uboot",
            out.getvalue().strip())

        name, pwid, link_name = cser.get_project()
        self.assertEqual('U-Boot', name)
        self.assertEqual(6, pwid)
        self.assertEqual('uboot', link_name)

        with terminal.capture() as (out, _):
            self.run_args('-P', 'https://url', 'patchwork', 'get-project')
        self.assertEqual(
            f"Project 'U-Boot' patchwork-ID {PROJ_ID} link-name uboot",
            out.getvalue().strip())

    def check_series_list_patches(self):
        """Test listing the patches for a series"""
        cser = self.get_cser()
        with terminal.capture() as (out, _):
            cser.add_series(None, '', allow_unmarked=True)
            cser.add_series('second', allow_unmarked=True)
            target = self.repo.lookup_reference('refs/heads/second')
            self.repo.checkout(target, strategy=pygit2.GIT_CHECKOUT_FORCE)
            cser.increment('second')

        with terminal.capture() as (out, _):
            yield cser
        lines = iter(out.getvalue().splitlines())
        self.assertEqual("Branch 'first' (total 2): 2:unknown", next(lines))
        self.assertIn('PatchId', next(lines))
        self.assertRegex(next(lines), r'  0 .* i2c: I2C things')
        self.assertRegex(next(lines), r'  1 .* spi: SPI fixes')

        with terminal.capture() as (out, _):
            yield cser
        lines = iter(out.getvalue().splitlines())
        self.assertEqual("Branch 'second2' (total 3): 3:unknown", next(lines))
        self.assertIn('PatchId', next(lines))
        self.assertRegex(next(lines), '  0 .* video: Some video improvements')
        self.assertRegex(next(lines), '  1 .* serial: Add a serial driver')
        self.assertRegex(next(lines), '  2 .* bootm: Make it boot')
        yield cser

    def test_series_list_patches(self):
        """Test listing the patches for a series"""
        cor = self.check_series_list_patches()
        cser = next(cor)
        cser.list_patches('first', 1)
        cser = next(cor)
        cser.list_patches('second2', 2)
        cser = next(cor)
        cor.close()

    def test_series_list_patches_cmdline(self):
        """Test listing the patches for a series using the cmdline"""
        cor = self.check_series_list_patches()
        cser = next(cor)
        self.run_args('series',  '-s', 'first', 'patches', pwork=True)
        cser.list_patches('first', 1)
        cser = next(cor)
        cser.list_patches('second2', 2)
        cser = next(cor)
        cor.close()

    def test_series_list_patches_detail(self):
        """Test listing the patches for a series"""
        cser = self.get_cser()
        with terminal.capture():
            cser.add_series(None, '', allow_unmarked=True)
            cser.add_series('second', allow_unmarked=True)
            target = self.repo.lookup_reference('refs/heads/second')
            self.repo.checkout(target, strategy=pygit2.GIT_CHECKOUT_FORCE)
            cser.increment('second')

        with terminal.capture() as (out, _):
            cser.list_patches('first', 1, show_commit=True)
        expect = r'''Branch 'first' (total 2): 2:unknown
Seq State      Com PatchId Commit     Subject
  0 unknown      -         .* i2c: I2C things

commit .*
Author: Test user <test@email.com>
Date:   .*

    i2c: I2C things

    This has some stuff to do with I2C

 i2c.c | 2 ++
 1 file changed, 2 insertions(+)


  1 unknown      -         .* spi: SPI fixes

commit .*
Author: Test user <test@email.com>
Date:   .*

    spi: SPI fixes

    SPI needs some fixes
    and here they are

    Signed-off-by: Lord Edmund Blackaddër <weasel@blackadder.org>

    Series-to: u-boot
    Commit-notes:
    title of the series
    This is the cover letter for the series
    with various details
    END

 spi.c | 3 +++
 1 file changed, 3 insertions(+)
'''
        lines = iter(out.getvalue().splitlines())
        for seq, eline in enumerate(expect.splitlines()):
            line = next(lines).rstrip()
            if '*' in eline:
                self.assertRegex(line, eline, f'line {seq + 1}')
            else:
                self.assertEqual(eline, line, f'line {seq + 1}')

        # Show just the patch; this should exclude the commit message
        with terminal.capture() as (out, _):
            cser.list_patches('first', 1, show_patch=True)
        chk = out.getvalue()
        self.assertIn('SPI fixes', chk)                 # subject
        self.assertNotIn('SPI needs some fixes', chk)   # commit body
        self.assertIn('make SPI work', chk)             # patch body

        # Show both
        with terminal.capture() as (out, _):
            cser.list_patches('first', 1, show_commit=True, show_patch=True)
        chk = out.getvalue()
        self.assertIn('SPI fixes', chk)                 # subject
        self.assertIn('SPI needs some fixes', chk)   # commit body
        self.assertIn('make SPI work', chk)             # patch body

    '''
    def _check_series_status(self, out):
        lines = iter(out.getvalue().splitlines())
        self.assertEqual(
            "Branch 'second' (total 3): 3:unknown", next(lines))
        self.assertIn('PatchId', next(lines))
        self.assertRegex(
            next(lines),
            "  0 unknown      -       .* video: Some video improvements")
        self.assertRegex(
            next(lines),
            "  1 unknown      -       .* serial: Add a serial driver")
        self.assertRegex(
            next(lines),
            "  2 unknown      -       .* bootm: Make it boot")

    def test_series_status(self):
        cser = self.get_cser()
        with terminal.capture():
            cser.add_series('second', 'description', allow_unmarked=True)
        with terminal.capture() as (out, _):
            cser.series_status('second', None)
        self._check_series_status(out)

    def test_series_status_cmdline(self):
        cser = self.get_cser()
        with terminal.capture():
            cser.add_series('second', 'description', allow_unmarked=True)
        with terminal.capture() as (out, _):
            self.run_args('series',  '-s', 'second', 'status', pwork=True)
        self._check_series_status(out)
    '''

    def test_series_sync(self):
        cser = self.get_cser()
        pwork = Patchwork.for_testing(self._fake_patchwork_cser)
        self.assertFalse(cser.get_project())
        cser.set_project(pwork, 'U-Boot', quiet=True)

        with terminal.capture() as (out, _):
            cser.add_series('second', 'description', allow_unmarked=True)
        with terminal.capture() as (out, _):
            cser.series_sync(pwork, 'second', None)
        lines = out.getvalue().splitlines()
        self.assertEqual(
            "Updating series 'second' version 1 from link '183237'",
            lines[0])
        self.assertEqual('3 patches and cover letter updated', lines[1])
        self.assertEqual(2, len(lines))

        ser = cser.get_series_by_name('second')
        pwid = cser.get_series_svid(ser.idnum, 1)
        pwc = cser.get_pcommit_dict(pwid)
        self.assertEqual('accepted', pwc[0].state)
        self.assertEqual('changes-requested', pwc[1].state)
        self.assertEqual('rejected', pwc[2].state)

    def test_series_sync_all(self):
        """Sync all series at once"""
        cser, pwork = self.setup_second(False)

        with terminal.capture():
            cser.add_series('first', 'description', allow_unmarked=True)
            cser.increment('first')
            cser.increment('first')
            cser.set_link('first', 1, '123', True)
            cser.set_link('first', 2, '1234', True)
            cser.set_link('first', 3, '31', True)
            cser.autolink(pwork, 'second', 2, True)

        with terminal.capture() as (out, _):
            cser.series_sync_all(pwork)
        self.assertEqual(
            '5 patches and 2 cover letters updated, 0 missing links (16 requests)',
            out.getvalue().strip())

        with terminal.capture() as (out, _):
            cser.series_sync_all(pwork, sync_all_versions=True)
        self.assertEqual(
            '12 patches and 5 cover letters updated, 0 missing links (40 requests)',
            out.getvalue().strip())

    def _check_second(self, lines, show_all):
        self.assertEqual('second: Series for my board (versions: 1 2)',
                         next(lines))
        if show_all:
            self.assertEqual("Branch 'second' (total 3): 3:unknown",
                             next(lines))
            self.assertIn('PatchId', next(lines))
            self.assertRegex(
                next(lines),
                '  0 unknown      -         .* video: Some video improvements')
            self.assertRegex(
                next(lines),
                '  1 unknown      -         .* serial: Add a serial driver')
            self.assertRegex(
                next(lines),
                '  2 unknown      -         .* bootm: Make it boot')
            self.assertEqual('', next(lines))
        self.assertEqual(
            "Branch 'second2' (total 3): 1:accepted 1:changes 1:rejected",
            next(lines))
        self.assertIn('PatchId', next(lines))
        self.assertEqual(
            'Cov              2      39            The name of the cover letter',
            next(lines))
        self.assertRegex(
            next(lines),
            '  0 accepted     2      10 .* video: Some video improvements')
        self.assertRegex(
            next(lines),
            '  1 changes             11 .* serial: Add a serial driver')
        self.assertRegex(
            next(lines),
            '  2 rejected     3      12 .* bootm: Make it boot')

    def test_series_progress(self):
        """Test showing progress for a cseries"""
        self.setup_second()

        args = Namespace(subcmd='progress', series='second', extra = [],
                         show_all_versions=False, list_patches=True)
        self.db_close()
        with terminal.capture() as (out, _):
            control.do_series(args, test_db=self.tmpdir, pwork=True)
        lines = iter(out.getvalue().splitlines())
        self._check_second(lines, False)

        args.show_all_versions = True
        with terminal.capture() as (out, _):
            control.do_series(args, test_db=self.tmpdir, pwork=True)
        lines = iter(out.getvalue().splitlines())
        self._check_second(lines, True)

    def _check_first(self, lines):
        self.assertEqual('first:  (versions: 1)', next(lines))
        self.assertEqual("Branch 'first' (total 2): 2:unknown", next(lines))
        self.assertIn('PatchId', next(lines))
        self.assertRegex(
            next(lines),
            '  0 unknown      -        .* i2c: I2C things')
        self.assertRegex(
            next(lines),
            '  1 unknown      -        .* spi: SPI fixes')
        self.assertEqual('', next(lines))

    def test_series_progress_all(self):
        """Test showing progress for all cseries"""
        self.setup_second()

        self.db_close()
        args = Namespace(subcmd='progress', series=None, extra = [],
                         show_all_versions=False, list_patches=True)
        with terminal.capture() as (out, _):
            control.do_series(args, test_db=self.tmpdir, pwork=True)
        lines = iter(out.getvalue().splitlines())
        self._check_first(lines)
        self._check_second(lines, False)

        args.show_all_versions = True
        with terminal.capture() as (out, _):
            control.do_series(args, test_db=self.tmpdir, pwork=True)
        lines = iter(out.getvalue().splitlines())
        self._check_first(lines)
        self._check_second(lines, True)

    def test_series_progress_no_patches(self):
        """Test showing progress for all cseries without patches"""
        self.setup_second()

        with terminal.capture() as (out, _):
            self.run_args('series', 'progress', pwork=True)
        lines = iter(out.getvalue().splitlines())
        self.assertEqual(
            'Name             Description                               Count  Status',
            next(lines))
        self.assertTrue(next(lines).startswith('--'))
        self.assertEqual(
            'first                                                          2  2:unknown',
            next(lines))
        self.assertEqual(
            'second2          The name of the cover letter                  3  '
            '1:accepted 1:changes 1:rejected',
            next(lines))
        self.assertTrue(next(lines).startswith('--'))
        self.assertEqual(
            ['2', 'series', '5', '2:unknown', '1:accepted', '1:changes', '1:rejected'],
            next(lines).split())
        self.assertFinished(lines)

    def test_series_progress_all_no_patches(self):
        """Test showing progress for all cseries versions without patches"""
        self.setup_second()

        with terminal.capture() as (out, _):
            self.run_args('series', 'progress', '--show-all-versions',
                          pwork=True)
        lines = iter(out.getvalue().splitlines())
        self.assertEqual(
            'Name             Description                               Count  Status',
            next(lines))
        self.assertTrue(next(lines).startswith('--'))
        self.assertEqual(
            'first                                                          2  2:unknown',
            next(lines))
        self.assertEqual(
            'second                                                         3  3:unknown',
            next(lines))
        self.assertEqual(
            'second2          The name of the cover letter                  3  '
            '1:accepted 1:changes 1:rejected',
            next(lines))
        self.assertTrue(next(lines).startswith('--'))
        self.assertEqual(
            ['3', 'series', '8', '5:unknown', '1:accepted', '1:changes', '1:rejected'],
            next(lines).split())
        self.assertFinished(lines)

    def test_series_summary(self):
        self.setup_second()

        self.db_close()
        args = Namespace(subcmd='summary', series=None, extra = [])
        with terminal.capture() as (out, _):
            control.do_series(args, test_db=self.tmpdir, pwork=True)
        lines = out.getvalue().splitlines()
        self.assertEqual(
            'Name               Status  Description',
            lines[0])
        self.assertEqual(
            '-----------------  ------  ------------------------------',
            lines[1])
        self.assertEqual('first          -/2  ', lines[2])
        self.assertEqual('second         1/3  Series for my board', lines[3])

    def test_series_open(self):
        cser = self.get_cser()
        pwork = Patchwork.for_testing(self._fake_patchwork_cser_link)
        self.assertFalse(cser.get_project())
        pwork.set_project(PROJ_ID, PROJ_LINK_NAME)

        with terminal.capture():
            cser.add_series('second', allow_unmarked=True)
            cser.increment('second')
            cser.autolink(pwork, 'second', 2, True)
            cser.series_sync(pwork, 'second', 2)

        with mock.patch.object(cros_subprocess.Popen, '__init__',
                               return_value=None) as method:
            with terminal.capture() as (out, _):
                cser.open_series(pwork, 'second2', 2)

        url = 'https://patchwork.ozlabs.org/project/uboot/list/?series=457&state=*&archive=both'
        method.assert_called_once_with(['xdg-open', url])
        self.assertEqual(f'Opening {url}', out.getvalue().strip())

    def test_name_version(self):
        """Test handling of series names and versions"""
        cser = self.get_cser()
        repo = self.repo

        self.assertEqual(('fred', None), cser.split_name_version('fred'))
        self.assertEqual(('mary', 2), cser.split_name_version('mary2'))

        ser, version = cser.parse_series_and_version(None, None)
        self.assertEqual('first', ser.name)
        self.assertEqual(1, version)

        ser, version = cser.parse_series_and_version('first', None)
        self.assertEqual('first', ser.name)
        self.assertEqual(1, version)

        ser, version = cser.parse_series_and_version('first', 2)
        self.assertEqual('first', ser.name)
        self.assertEqual(2, version)

        with self.assertRaises(ValueError) as exc:
            cser.parse_series_and_version('123', 2)
        self.assertEqual(
            "Series name '123' cannot be a number, use '<name><version>'",
            str(exc.exception))

        with self.assertRaises(ValueError) as exc:
            cser.parse_series_and_version('first', 100)
        self.assertEqual("Version 100 exceeds 99", str(exc.exception))

        with self.assertRaises(ValueError) as exc:
            cser.parse_series_and_version('mary3', 4)
        self.assertEqual(
            'Version mismatch: -V has 4 but branch name indicates 3',
            str(exc.exception))

        # Move off the branch and check for a sensible error
        commit = repo.revparse_single('first~')
        repo.checkout_tree(commit)
        repo.set_head(commit.oid)

        with self.assertRaises(ValueError) as exc:
            cser.parse_series_and_version(None, None)
        self.assertEqual('No branch detected: please use -s <series>',
                         str(exc.exception))

    def test_name_version_extra(self):
        """More tests for some corner cases"""
        cser, _ = self.setup_second()

        ser, version = cser.parse_series_and_version(None, None)
        self.assertEqual('second', ser.name)
        self.assertEqual(2, version)

        ser, version = cser.parse_series_and_version('second2', None)
        self.assertEqual('second', ser.name)
        self.assertEqual(2, version)

    def test_migrate(self):
        """Test migration to later schema versions"""
        db = database.Database(f'{self.tmpdir}/.patman.db')
        with terminal.capture() as (out, err):
            db.open_it()
        self.assertEqual(
            f'Creating new database {self.tmpdir}/.patman.db',
            err.getvalue().strip())

        self.assertEqual(0, db.get_schema_version())

        for version in range(1, database.LATEST + 1):
            with terminal.capture() as (out, _):
                db.migrate_to(version)
            self.assertTrue(os.path.exists(
                f'{self.tmpdir}/.patman.dbold.v{version - 1}'))
            self.assertEqual(f'Update database to v{version}',
                             out.getvalue().strip())
            self.assertEqual(version, db.get_schema_version())
        self.assertEqual(3, database.LATEST)

    def test_series_scan(self):
        """Test scanning a series for updates"""
        cser, _ = self.setup_second()

        # Add a new commit
        self.repo = pygit2.init_repository(self.gitdir)
        self.make_commit_with_file(
            'wip: Try out a new thing', 'Just checking', 'wibble.c',
            '''changes to wibble''')
        target = self.repo.revparse_single('HEAD')
        self.repo.reset(target.oid, pygit2.enums.ResetMode.HARD)

        name = gitutil.get_branch(self.gitdir)
        # upstream_name = gitutil.get_upstream(self.gitdir, name)
        name, ser, version, _ = cser._prep_series(None)

        # We now have 4 commits numbered 0 (second~3) to 3 (the one we just
        # added). Drop commit 1 from the branch
        cser.filter_commits(name, ser, 1)
        svid = cser.get_ser_ver(ser.idnum, version)[0]
        old_pcdict = cser.get_pcommit_dict(svid).values()

        expect = '''Syncing series 'second2' v2: mark False allow_unmarked True
    0 video: Some video improvements
-   1 serial: Add a serial driver
    1 bootm: Make it boot
+   2 Just checking
'''
        with terminal.capture() as (out, _):
            self.run_args('series', '-n', 'scan', '-M', pwork=True)
        self.assertEqual(expect + 'Dry run completed\n', out.getvalue())

        new_pcdict = cser.get_pcommit_dict(svid).values()
        self.assertEqual(list(old_pcdict), list(new_pcdict))

        with terminal.capture() as (out, _):
            self.run_args('series', 'scan', '-M', pwork=True)
        self.assertEqual(expect, out.getvalue())

        new_pcdict = cser.get_pcommit_dict(svid).values()
        self.assertEqual(len(old_pcdict), len(new_pcdict))
        chk = list(new_pcdict)
        self.assertNotEqual(list(old_pcdict), list(new_pcdict))
        self.assertEqual('video: Some video improvements', chk[0].subject)
        self.assertEqual('bootm: Make it boot', chk[1].subject)
        self.assertEqual('Just checking', chk[2].subject)

    def test_series_send(self):
        """Test sending a series"""
        cser, pwork = self.setup_second()

        # Create a third version
        with terminal.capture():
            cser.increment('second')
        series = patchstream.get_metadata_for_list('second3', self.gitdir, 3)
        self.assertEqual('2:457', series.links)
        self.assertEqual('3', series.version)

        with terminal.capture() as (out, err):
            self.run_args('series', '-n', 'send', '--no-autolink', pwork=pwork)
        lines = out.getvalue().splitlines()
        err_lines = err.getvalue().splitlines()
        self.assertIn('Send a total of 3 patches with a cover letter',
                      out.getvalue())
        self.assertIn('video.c:1: warning: Missing or malformed SPDX-License-Identifier tag in line 1',
                      err.getvalue())
        self.assertIn('<patch>:19: warning: added, moved or deleted file(s), does MAINTAINERS need updating?',
                      err.getvalue())
        self.assertIn('bootm.c:1: check: Avoid CamelCase: <Fix>',
                      err.getvalue())
        self.assertIn('Cc:  Anatolij Gustschin <agust@denx.de>', out.getvalue())

        self.assertTrue(os.path.exists(os.path.join(
            self.tmpdir, '0001-video-Some-video-improvements.patch')))
        self.assertTrue(os.path.exists(os.path.join(
            self.tmpdir, '0002-serial-Add-a-serial-driver.patch')))
        self.assertTrue(os.path.exists(os.path.join(
            self.tmpdir, '0003-bootm-Make-it-boot.patch')))

    def test_series_send_and_link(self):
        """Test sending a series and then adding its link to the database"""
        def h_sleep(time_s):
            if cser.get_time() > 100:
                self.autolink_extra = {'id': 500, 'name': 'Series for my board',
                                       'version': 3}
            cser.inc_fake_time(time_s)

        cser, pwork = self.setup_second()

        # Create a third version
        with terminal.capture():
            cser.increment('second')
        series = patchstream.get_metadata_for_list('second3', self.gitdir, 3)
        self.assertEqual('2:457', series.links)
        self.assertEqual('3', series.version)

        with terminal.capture():
            self.run_args('series', '-n', 'send', pwork=pwork)

        cser.set_fake_time(h_sleep)
        with terminal.capture() as (out, _):
            cser.autolink(pwork, 'second3', 3, True, 200)
        lines = iter(out.getvalue().splitlines())
        for i in range(7):
            self.assertEqual(
                "Possible matches for 'second' v3 desc 'Series for my board':",
                 next(lines), f'failed at i={i}')
            self.assertEqual('  Link  Version  Description', next(lines))
            self.assertEqual('   456        1  Series for my board', next(lines))
            self.assertEqual('   457        2  Series for my board', next(lines))
            self.assertEqual('Sleeping for 20 seconds', next(lines))
        self.assertEqual('Link completed after 140 seconds', next(lines))
        self.assertEqual('Checking out upstream commit refs/heads/base',
                         next(lines))
        self.assertEqual(
            "Processing 3 commits from branch 'second3'", next(lines))
        self.assertRegex(
            next(lines),
            "-  .* as .*: video: Some video improvements")
        self.assertRegex(
            next(lines),
            "- added links '3:500 2:457'  .* as .*: serial: Add a serial driver")
        self.assertRegex(
            next(lines),
            "- added version 3 .* as .*: bootm: Make it boot")
        self.assertRegex(
            next(lines),
            "Updating branch second3 to .*")
        self.assertEqual(
            "Setting link for series 'second' v3 to 500", next(lines))

    def _check_status(self, out, has_comments, has_cover_comments):
        lines = iter(out.getvalue().splitlines())
        if has_cover_comments:
            self.assertEqual('Cov The name of the cover letter', next(lines))
            self.assertEqual(
                'From: A user <user@user.com>: Sun 13 Apr 14:06:02 MDT 2025',
                next(lines))
            self.assertEqual('some comment', next(lines))
            self.assertEqual('', next(lines))

            self.assertEqual(
                'From: Ghenkis Kham <gk@eurasia.gov>: Sun 13 Apr 13:06:02 MDT 2025',
                next(lines))
            self.assertEqual('another comment', next(lines))
            self.assertEqual('', next(lines))

        self.assertEqual('  1 video: Some video improvements', next(lines))
        self.assertEqual('  + Reviewed-by: Fred Bloggs <fred@bloggs.com>',
                         next(lines))
        if has_comments:
            self.assertEqual(
                'Review: Fred Bloggs <fred@bloggs.com>', next(lines))
            self.assertEqual('    > This was my original patch', next(lines))
            self.assertEqual('    > which is being quoted', next(lines))
            self.assertEqual(
                '    I like the approach here and I would love to see more of it.',
                next(lines))
            self.assertEqual('', next(lines))

        self.assertEqual('  2 serial: Add a serial driver', next(lines))
        self.assertEqual('  3 bootm: Make it boot', next(lines))
        self.assertEqual(
            '1 new response available in patchwork (use -d to write them to a new branch)',
            next(lines))

    def test_series_status(self):
        """Test getting the status of a series, including comments"""
        cser, pwork = self.setup_second()

        # Use single threading for easy debugging, but the multithreaded version
        # should produce the same output
        with terminal.capture() as (out, _):
            cser.series_status(pwork, 'second', 2, False, single_thread=True)
        self._check_status(out, False, False)
        self.loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.loop)

        with terminal.capture() as (out2, _):
            cser.series_status(pwork, 'second', 2, False, single_thread=False)
        self.assertEqual(out.getvalue(), out2.getvalue())
        self._check_status(out, False, False)

        with terminal.capture() as (out, _):
            cser.series_status(pwork, 'second', 2, show_comments=True,
                               single_thread=False)
        self._check_status(out, True, False)

        with terminal.capture() as (out, _):
            cser.series_status(pwork, 'second', 2, show_comments=True,
                               show_cover_comments=True, single_thread=False)
        self._check_status(out, True, True)

    def test_series_status_cmdline(self):
        """Test getting the status of a series, including comments"""
        cser, pwork = self.setup_second()

        # Use single threading for easy debugging, but the multithreaded version
        # should produce the same output
        with terminal.capture() as (out, _):
            self.run_args('series', '-s' 'second', '-V', '2', 'status',
                          pwork=pwork)
        self._check_status(out, False, False)

        with terminal.capture() as (out, _):
            cser.series_status(pwork, 'second', 2, show_comments=True,
                               single_thread=False)
        self._check_status(out, True, False)

        with terminal.capture() as (out, _):
            cser.series_status(pwork, 'second', 2, show_comments=True,
                               show_cover_comments=True, single_thread=False)
        self._check_status(out, True, True)

    def test_series_no_subcmd(self):
        """Test handling of things without a subcommand"""
        parsers = cmdline.setup_parser()
        parsers['series'].catch_error = True
        with terminal.capture() as (out, _):
            cmdline.parse_args(['series'], parsers=parsers)
        self.assertIn('usage: patman series', out.getvalue())

        parsers['patchwork'].catch_error = True
        with terminal.capture() as (out, _):
            cmdline.parse_args(['patchwork'], parsers=parsers)
        self.assertIn('usage: patman patchwork', out.getvalue())

        parsers['upstream'].catch_error = True
        with terminal.capture() as (out, _):
            cmdline.parse_args(['upstream'], parsers=parsers)
        self.assertIn('usage: patman upstream', out.getvalue())
