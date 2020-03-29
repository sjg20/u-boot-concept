# SPDX-License-Identifier: GPL-2.0+
# Copyright 2020 Google LLC
# Written by Simon Glass <sjg@chromium.org>
#
# Command-line parser for labman
#

from argparse import ArgumentParser

def ParseArgs(argv):
    """Parse the labman command-line arguments

    Args:
        argv: List of string arguments
    Returns:
        Tuple (options, args) with the command-line options and arugments.
            options provides access to the options (e.g. option.debug)
            args is a list of string arguments
    """
    epilog = '''Labman managers your hardware lab.'''

    parser = ArgumentParser(epilog=epilog)
    parser.add_argument('-D', '--debug', action='store_true',
        help='Enabling debugging (provides a full traceback on error)')
    parser.add_argument('-l', '--lab', type=str,
        help='Select the lab description to use (yaml file)')

    subparsers = parser.add_subparsers(dest='cmd')
    build_parser = subparsers.add_parser('ls', help='Show lab info')

    return parser.parse_args(argv)
