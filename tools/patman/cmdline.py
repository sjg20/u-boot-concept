# SPDX-License-Identifier: GPL-2.0+
#
# Copyright 2023 Google LLC
#

"""Handles parsing of buildman arguments

This creates the argument parser and uses it to parse the arguments passed in
"""

import argparse
import os
import pathlib
import sys

from patman import project
from u_boot_pylib import gitutil
from patman import settings

PATMAN_DIR = pathlib.Path(__file__).parent
HAS_TESTS = os.path.exists(PATMAN_DIR / "func_test.py")


class ErrorCatchingArgumentParser(argparse.ArgumentParser):
    def __init__(self, **kwargs):
        self.catch_error = False
        super().__init__(**kwargs)

    def exit(self, status=0, message=None):
        if self.catch_error:
            raise ValueError('Bad argument')
        exit(status)


def add_send_args(par):
    par.add_argument('-i', '--ignore-errors', action='store_true',
           dest='ignore_errors', default=False,
           help='Send patches email even if patch errors are found')
    par.add_argument('-l', '--limit-cc', dest='limit', type=int, default=None,
           help='Limit the cc list to LIMIT entries [default: %(default)s]')
    par.add_argument('-m', '--no-maintainers', action='store_false',
           dest='add_maintainers', default=True,
           help="Don't cc the file maintainers automatically")
    par.add_argument(
        '--get-maintainer-script', dest='get_maintainer_script', type=str,
        action='store',
        default=os.path.join(gitutil.get_top_level(), 'scripts',
                             'get_maintainer.pl') + ' --norolestats',
        help='File name of the get_maintainer.pl (or compatible) script.')
    par.add_argument('-r', '--in-reply-to', type=str, action='store',
                      help="Message ID that this series is in reply to")
    par.add_argument('-t', '--ignore-bad-tags', action='store_true',
                      default=False,
                      help='Ignore bad tags / aliases (default=warn)')
    par.add_argument('-T', '--thread', action='store_true', dest='thread',
                      default=False, help='Create patches as a single thread')
    par.add_argument('--no-binary', action='store_true', dest='ignore_binary',
                      default=False,
                      help="Do not output contents of changes in binary files")
    par.add_argument('--no-check', action='store_false', dest='check_patch',
                      default=True,
                      help="Don't check for patch compliance")
    par.add_argument(
        '--tree', dest='check_patch_use_tree', default=False,
        action='store_true',
        help=("Set `tree` to True. If `tree` is False then we'll pass "
              "'--no-tree' to checkpatch (default: tree=%(default)s)"))
    par.add_argument('--no-tree', dest='check_patch_use_tree',
                      action='store_false', help="Set `tree` to False")
    par.add_argument(
        '--no-tags', action='store_false', dest='process_tags', default=True,
        help="Don't process subject tags as aliases")
    par.add_argument('--no-signoff', action='store_false', dest='add_signoff',
                      default=True, help="Don't add Signed-off-by to patches")
    par.add_argument('--smtp-server', type=str,
                      help="Specify the SMTP server to 'git send-email'")
    par.add_argument('--keep-change-id', action='store_true',
                      help='Preserve Change-Id tags in patches to send.')

def parse_args(argv=None):
    """Parse command line arguments from sys.argv[]

    Args:
        argv (str or None): Arguments to process, or None to use sys.argv[1:]

    Returns:
        tuple containing:
            options: command line options
            args: command lin arguments
    """
    epilog = '''Create patches from commits in a branch, check them and email
        them as specified by tags you place in the commits. Use -n to do a dry
        run first.'''

    # parser = argparse.ArgumentParser(epilog=epilog)
    parser = ErrorCatchingArgumentParser(epilog=epilog)
    parser.add_argument('-D', '--debug', action='store_true',
        help='Enabling debugging (provides a full traceback on error)')
    parser.add_argument('-N', '--no-capture', action='store_true',
        help='Disable capturing of console output in tests')
    parser.add_argument('-p', '--project', default=project.detect_project(),
                        help="Project name; affects default option values and "
                        "aliases [default: %(default)s]")
    parser.add_argument('-P', '--patchwork-url',
                        default='https://patchwork.ozlabs.org',
                        help='URL of patchwork server [default: %(default)s]')
    parser.add_argument(
        '-v', '--verbose', action='store_true', dest='verbose', default=False,
        help='Verbose output of errors and warnings')
    parser.add_argument(
        '-X', '--test-preserve-dirs', action='store_true',
        help='Preserve and display test-created directories')
    parser.add_argument(
        '-H', '--full-help', action='store_true', dest='full_help',
        default=False, help='Display the README file')

    subparsers = parser.add_subparsers(dest='cmd')
    send = subparsers.add_parser(
        'send', help='Format, check and email patches (default command)')
    send.add_argument('-b', '--branch', type=str,
        help="Branch to process (by default, the current branch)")
    send.add_argument('-c', '--count', dest='count', type=int,
        default=-1, help='Automatically create patches from top n commits')
    send.add_argument('-e', '--end', type=int, default=0,
        help='Commits to skip at end of patch list')
    send.add_argument('-n', '--dry-run', action='store_true', dest='dry_run',
           default=False, help="Do a dry run (create but don't email patches)")
    send.add_argument('-s', '--start', dest='start', type=int,
        default=0, help='Commit to start creating patches from (0 = HEAD)')
    send.add_argument('--cc-cmd', dest='cc_cmd', type=str, action='store',
           default=None, help='Output cc list for patch file (used by git)')
    add_send_args(send)
    send.add_argument('patchfiles', nargs='*')

    # Only add the 'test' action if the test data files are available.
    if HAS_TESTS:
        test_parser = subparsers.add_parser('test', help='Run tests')
        test_parser.add_argument('testname', type=str, default=None, nargs='?',
                                 help="Specify the test to run")

    status = subparsers.add_parser('status',
                                   help='Check status of patches in patchwork')
    status.add_argument('-C', '--show-comments', action='store_true',
                        help='Show comments from each patch')
    status.add_argument(
        '-d', '--dest-branch', type=str,
        help='Name of branch to create with collected responses')
    status.add_argument('-f', '--force', action='store_true',
                        help='Force overwriting an existing branch')

    series = subparsers.add_parser('series', help='Manage series of patches')
    series.no_defaults = True
    series.add_argument('-a', '--all', action='store_true',
                        help='Show all series versions, not just the last')
    series.add_argument('-n', '--dry-run', action='store_true', dest='dry_run',
            default=False, help="Do a dry run (create but don't email patches)")
    series.add_argument('-s', '--series', help='Name of series')
    series.add_argument('-U', '--upstream', help='Commit to end before')
    series.add_argument('-V', '--version', type=int,
                        help='Version number to link')
    series_subparsers = series.add_subparsers(dest='subcmd')
    add = series_subparsers.add_parser('add')
    add.add_argument('-d', '--desc',
                     help='Series description / cover-letter title')
    add.add_argument('-m', '--mark', action='store_true',
                     help='Mark unmarked commits with a Change-Id field')
    add.add_argument('-M', '--allow-unmarked', action='store_true',
                     help="Don't require commits to be marked")
    series_subparsers.add_parser('archive')
    auto = series_subparsers.add_parser('auto-link')
    auto.add_argument('-u', '--update', action='store_true',
                      help='Update the branch commit')
    series_subparsers.add_parser('dec')
    series_subparsers.add_parser('get-link')
    series_subparsers.add_parser('inc')
    series_subparsers.add_parser('list')
    series_subparsers.add_parser('open')
    series_subparsers.add_parser('patches')
    series_subparsers.add_parser('progress')
    series_subparsers.add_parser('remove')
    series_subparsers.add_parser('remove-version')
    scan = series_subparsers.add_parser('scan')
    scan.add_argument('-m', '--mark', action='store_true',
                      help='Mark unmarked commits with a Change-Id field')
    scan.add_argument('-M', '--allow-unmarked', action='store_true',
                      help="Don't require commits to be marked")
    ssend = series_subparsers.add_parser('publish', aliases=['pub'])
    add_send_args(ssend)
    setl = series_subparsers.add_parser('set-link')
    setl.add_argument('-u', '--update', action='store_true',
                      help='Update the branch commit')
    setl.add_argument(
        'link', help='Link to use, i.e. patchwork series number (e.g. 452329)')
    series_subparsers.add_parser('summary')
    series_subparsers.add_parser('sync')
    series_subparsers.add_parser('unarchive')
    series_subparsers.add_parser('unmark')

    upstream = subparsers.add_parser('upstream', aliases=['us'],
                                     help='Manage upstream destinations')
    upstream.no_defaults = True
    upstream_subparsers = upstream.add_subparsers(dest='subcmd')
    uadd = upstream_subparsers.add_parser('add')
    uadd.add_argument(
        'remote_name', help="Git remote name used for this upstream, e.g. 'us'")
    uadd.add_argument(
        'url', help="URL to use for this upstream, e.g. 'https://gitlab.denx.de/u-boot/u-boot.git'")
    udel = upstream_subparsers.add_parser('delete')
    udel.add_argument(
        'remote_name', help="Git remote name used for this upstream, e.g. 'us'")
    upstream_subparsers.add_parser('list')
    udef = upstream_subparsers.add_parser('default')
    udef.add_argument('-u', '--unset', action='store_true',
                      help='Unset the default upstream')
    udef.add_argument('remote_name', nargs='?',
                      help="Git remote name used for this upstream, e.g. 'us'")

    patchwork = subparsers.add_parser('patchwork', aliases=['pw'],
                                      help='Manage patchwork connection')
    patchwork.no_defaults = True
    patchwork_subparsers = patchwork.add_subparsers(dest='subcmd')
    patchwork_subparsers.add_parser('get-project')
    uset = patchwork_subparsers.add_parser('set-project')
    uset.add_argument(
        'project_name', help="Patchwork project name, e.g. 'U-Boot'")

    # Parse options twice: first to get the project and second to handle
    # defaults properly (which depends on project)
    # Use parse_known_args() in case 'cmd' is omitted
    if not argv:
        argv = sys.argv[1:]

    args, rest = parser.parse_known_args(argv)
    if hasattr(args, 'project'):
        settings.Setup(parser, args.project, argv)
        print('args', argv)
        args, rest = parser.parse_known_args(argv)
        print('done')

    # If we have a command, it is safe to parse all arguments
    if args.cmd:
        args = parser.parse_args(argv)
    else:
        # No command, so insert it after the known arguments and before the ones
        # that presumably relate to the 'send' subcommand
        nargs = len(rest)
        argv = argv[:-nargs] + ['send'] + rest
        args = parser.parse_args(argv)

    print('process_tags', args.process_tags)

    return args
