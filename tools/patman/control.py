# SPDX-License-Identifier: GPL-2.0+
#
# Copyright 2020 Google LLC
#
"""Handles the main control logic of patman

This module provides various functions called by the main program to implement
the features of patman.
"""

import re
import traceback

from patman import cseries
from patman import patchstream
from patman import send
from patman.patchwork import Patchwork
from u_boot_pylib import gitutil
from u_boot_pylib import terminal
from u_boot_pylib import tout


def setup():
    """Do required setup before doing anything"""
    gitutil.setup()


def do_send(args):
    """Create, check and send patches by email

    Args:
        args (argparse.Namespace): Arguments to patman
    """
    setup()
    send.send(args)


def patchwork_status(branch, count, start, end, dest_branch, force,
                     show_comments, url):
    """Check the status of patches in patchwork

    This finds the series in patchwork using the Series-link tag, checks for new
    comments and review tags, displays then and creates a new branch with the
    review tags.

    Args:
        branch (str): Branch to create patches from (None = current)
        count (int): Number of patches to produce, or -1 to produce patches for
            the current branch back to the upstream commit
        start (int): Start partch to use (0=first / top of branch)
        end (int): End patch to use (0=last one in series, 1=one before that,
            etc.)
        dest_branch (str): Name of new branch to create with the updated tags
            (None to not create a branch)
        force (bool): With dest_branch, force overwriting an existing branch
        show_comments (bool): True to display snippets from the comments
            provided by reviewers
        url (str): URL of patchwork server, e.g. 'https://patchwork.ozlabs.org'.
            This is ignored if the series provides a Series-patchwork-url tag.

    Raises:
        ValueError: if the branch has no Series-link value
    """
    if not branch:
        branch = gitutil.get_branch()
    if count == -1:
        # Work out how many patches to send if we can
        count = gitutil.count_commits_to_branch(branch) - start

    series = patchstream.get_metadata(branch, start, count - end)
    warnings = 0
    for cmt in series.commits:
        if cmt.warn:
            print('%d warnings for %s:' % (len(cmt.warn), cmt.hash))
            for warn in cmt.warn:
                print('\t', warn)
                warnings += 1
            print
    if warnings:
        raise ValueError('Please fix warnings before running status')
    links = series.get('links')
    if not links:
        raise ValueError("Branch has no Series-links value")

    _, version = cseries.split_name_version(branch)
    link = series.get_link_for_version(version, links)
    if not link:
        raise ValueError('Series-links has no link for v{version}')
    tout.debug(f"Link '{link}")

    # Allow the series to override the URL
    if 'patchwork_url' in series:
        url = series.patchwork_url

    # Import this here to avoid failing on other commands if the dependencies
    # are not present
    from patman import status
    pwork = Patchwork(url)
    status.check_and_report_patchwork_status(series, link, branch, dest_branch, force,
                                  show_comments, False, pwork)

def do_series(args, test_db=None, pwork=None):
    """Process a series subcommand

    Args:
        args (Namespace): Arguments to process
        test_db (str or None): Directory containing the test database, None to
            use the normal one
        pwork (Patchwork): Patchwork object to use
    """
    cser = cseries.Cseries(test_db)
    needs_patchwork = [
        'autolink', 'autolink-all', 'open', 'send', 'status', 'sync',
        'sync-all'
        ]
    try:
        cser.open_database()
        if not pwork and args.subcmd in needs_patchwork:
            pwork = Patchwork(args.patchwork_url)
            proj = cser.get_project()
            if not proj:
                raise ValueError(
                    "Please set project ID with 'patman patchwork set-project'")
            _, proj_id, link_name = cser.get_project()
            pwork.set_project(proj_id, link_name)
        if args.subcmd == 'add':
            cser.add_series(args.series, args.desc,
                            mark=args.mark, allow_unmarked=args.allow_unmarked,
                            end=args.upstream, dry_run=args.dry_run)
        elif args.subcmd == 'archive':
            cser.set_archived(args.series, True)
        elif args.subcmd == 'autolink':
            cser.autolink(pwork, args.series, args.version, args.update,
                              args.autolink_wait)
        elif args.subcmd == 'autolink-all':
            cser.autolink_all(pwork, update_commit=args.update,
                              link_all_versions=args.link_all_versions,
                              replace_existing=args.replace_existing,
                              dry_run=args.dry_run, show_summary=True)
        elif args.subcmd == 'dec':
            cser.decrement(args.series, args.dry_run)
        elif args.subcmd == 'get-link':
            link = cser.get_link(args.series, args.version)
            print(link)
        elif args.subcmd == 'inc':
            cser.increment(args.series, args.dry_run)
        elif args.subcmd == 'list':
            cser.series_list()
        elif args.subcmd == 'open':
            cser.open_series(pwork, args.series, args.version)
        elif args.subcmd == 'mark':
            cser.mark_series(args.series, args.allow_marked,
                             dry_run=args.dry_run)
        elif args.subcmd == 'patches':
            cser.list_patches(args.series, args.version, args.commit,
                              args.patch)
        elif args.subcmd == 'progress':
            cser.progress(args.series, args.show_all_versions,
                          args.list_patches)
        elif args.subcmd == 'remove':
            cser.remove_series(args.series, dry_run=args.dry_run)
        elif args.subcmd == 'remove-version':
            cser.remove_version(args.series, args.version, dry_run=args.dry_run)
        elif args.subcmd == 'rename':
            cser.series_rename(args.series, args.new_name, dry_run=args.dry_run)
        elif args.subcmd == 'scan':
            cser.scan(args.series, mark=args.mark,
                      allow_unmarked=args.allow_unmarked, end=args.upstream,
                      dry_run=args.dry_run)
        elif args.subcmd == 'send':
            cser.send_series(pwork, args.series, args.autolink,
                             args.autolink_wait, args)
        elif args.subcmd == 'set-link':
            cser.set_link(args.series, args.version, args.link, args.update)
        elif args.subcmd == 'status':
            cser.series_status(pwork, args.series, args.version,
                               args.show_comments, args.show_cover_comments)
        elif args.subcmd == 'summary':
            cser.summary(args.series)
        elif args.subcmd == 'sync':
            cser.series_sync(pwork, args.series, args.version,
                             args.show_comments, args.show_cover_comments,
                             args.gather_tags, dry_run=args.dry_run)
        elif args.subcmd == 'sync-all':
            cser.series_sync_all(pwork, args.sync_all_versions,
                                 args.gather_tags, args.dry_run)
        elif args.subcmd == 'unarchive':
            cser.set_archived(args.series, False)
        elif args.subcmd == 'unmark':
            cser.unmark_series(args.series, args.allow_unmarked,
                               dry_run=args.dry_run)
        else:
            raise ValueError(f"Unknown series subcommand '{args.subcmd}'")
    finally:
        cser.close_database()


def upstream(args, test_db=None):
    """Process an 'upstream' subcommand

    Args:
        args (Namespace): Arguments to process
        test_db (str or None): Directory containing the test database, None to
            use the normal one
    """
    cser = cseries.Cseries(test_db)
    try:
        cser.open_database()
        if args.subcmd == 'add':
            cser.add_upstream(args.remote_name, args.url)
        elif args.subcmd == 'default':
            if args.unset:
                cser.set_default_upstream(None)
            elif args.remote_name:
                cser.set_default_upstream(args.remote_name)
            else:
                result = cser.get_default_upstream()
                print(result if result else 'unset')
        elif args.subcmd == 'delete':
            cser.delete_upstream(args.remote_name)
        elif args.subcmd == 'list':
            cser.list_upstream()
        else:
            raise ValueError(f"Unknown upstream subcommand '{args.subcmd}'")
    finally:
        cser.close_database()


def patchwork(args, test_db=None, pwork=None):
    """Process a 'patchwork' subcommand
    Args:
        args (Namespace): Arguments to process
        test_db (str or None): Directory containing the test database, None to
            use the normal one
        pwork (Patchwork): Patchwork object to use
    """
    cser = cseries.Cseries(test_db)
    try:
        cser.open_database()
        if args.subcmd == 'set-project':
            if not pwork:
                pwork = Patchwork(args.patchwork_url)
            cser.set_project(pwork, args.project_name)
        elif args.subcmd == 'get-project':
            info = cser.get_project()
            if not info:
                raise ValueError("Project has not been set; use 'patman patchwork set-project'")
            name, pwid, link_name = info
            print(f"Project '{name}' patchwork-ID {pwid} link-name {link_name}")
        else:
            raise ValueError(f"Unknown patchwork subcommand '{args.subcmd}'")
    finally:
        cser.close_database()

def do_patman(args, test_db=None, pwork=None):
    if args.cmd == 'send':
        # Called from git with a patch filename as argument
        # Printout a list of additional CC recipients for this patch
        if args.cc_cmd:
            re_line = re.compile(r'(\S*) (.*)')
            with open(args.cc_cmd, 'r', encoding='utf-8') as inf:
                for line in inf.readlines():
                    match = re_line.match(line)
                    if match and match.group(1) == args.patchfiles[0]:
                        for cca in match.group(2).split('\0'):
                            cca = cca.strip()
                            if cca:
                                print(cca)

        elif args.full_help:
            with resources.path('patman', 'README.rst') as readme:
                tools.print_full_help(str(readme))
        else:
            # If we are not processing tags, no need to warning about bad ones
            if not args.process_tags:
                args.ignore_bad_tags = True
            do_send(args)

    ret_code = 0
    try:
        # Check status of patches in patchwork
        if args.cmd == 'status':
            patchwork_status(args.branch, args.count, args.start, args.end,
                             args.dest_branch, args.force, args.show_comments,
                             args.patchwork_url)
        elif args.cmd == 'series':
            do_series(args, test_db, pwork)
        elif args.cmd in ('upstream', 'us'):
            upstream(args, test_db)
        elif args.cmd in ('patchwork', 'pw'):
            patchwork(args, test_db, pwork)
    except Exception as exc:
        terminal.tprint(f'patman: {type(exc).__name__}: {exc}',
                        colour=terminal.Color.RED)
        if args.debug:
            print()
            traceback.print_exc()
        ret_code = 1
    return ret_code
