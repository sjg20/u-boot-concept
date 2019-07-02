# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2016 Google, Inc
# Written by Simon Glass <sjg@chromium.org>
#
# Command-line parser for binman
#

from argparse import ArgumentParser

def ParseArgs(argv):
    """Parse the binman command-line arguments

    Args:
        argv: List of string arguments
    Returns:
        Tuple (options, args) with the command-line options and arugments.
            options provides access to the options (e.g. option.debug)
            args is a list of string arguments
    """
    if '-H' in argv:
        argv.insert(0, 'build')

    usage = '''binman [-B <dir<] [-D] [-H] [-v<n>] [--toolpath <path>] <cmd> <options>

Create and manipulate images for a board from a set of binaries. Binman is
controlled by a description in the board device tree.'''

    base_parser = ArgumentParser(add_help=False)
    base_parser.add_argument('-B', '--build-dir', type=str, default='b',
        help='Directory containing the build output')
    base_parser.add_argument('-D', '--debug', action='store_true',
        help='Enabling debugging (provides a full traceback on error)')
    base_parser.add_argument('-H', '--full-help', action='store_true',
        default=False, help='Display the README file')
    base_parser.add_argument('--toolpath', type=str, action='append',
        help='Add a path to the directories containing tools')
    base_parser.add_argument('-v', '--verbosity', default=1,
        type=int, help='Control verbosity: 0=silent, 1=progress, 3=full, '
        '4=debug')

    parser = ArgumentParser(usage=usage, parents=[base_parser])
    subparsers = parser.add_subparsers(dest='cmd')

    build_parser = subparsers.add_parser('build', help='Build firmware image',
                                         parents=[base_parser])
    build_parser.add_argument('-a', '--entry-arg', type=str, action='append',
            help='Set argument value arg=value')
    build_parser.add_argument('-b', '--board', type=str,
            help='Board name to build')
    build_parser.add_argument('-d', '--dt', type=str,
            help='Configuration file (.dtb) to use')
    build_parser.add_argument('--fake-dtb', action='store_true',
            help='Use fake device tree contents (for testing only)')
    build_parser.add_argument('-i', '--image', type=str, action='append',
            help='Image filename to build (if not specified, build all)')
    build_parser.add_argument('-I', '--indir', action='append',
            help='Add a path to the list of directories to use for input files')
    build_parser.add_argument('-m', '--map', action='store_true',
        default=False, help='Output a map file for each image')
    build_parser.add_argument('-O', '--outdir', type=str,
        action='store', help='Path to directory to use for intermediate and '
        'output files')
    build_parser.add_argument('-p', '--preserve', action='store_true',\
        help='Preserve temporary output directory even if option -O is not '
             'given')
    build_parser.add_argument('-u', '--update-fdt', action='store_true',
        default=False, help='Update the binman node with offset/size info')

    entry_parser = subparsers.add_parser('entry-docs',
        help='Write out entry documentation (see README.entries)')

    list_parser = subparsers.add_parser('ls', help='List files in an image')
    list_parser.add_argument('fname', type=str, help='Image file to list')

    test_parser = subparsers.add_parser('test', help='Run tests',
                                        parents=[base_parser])
    test_parser.add_argument('-P', '--processes', type=int,
        help='set number of processes to use for running tests')
    test_parser.add_argument('-T', '--test-coverage', action='store_true',
        default=False, help='run tests and check for 100%% coverage')
    test_parser.add_argument('-X', '--test-preserve-dirs', action='store_true',
        help='Preserve and display test-created input directories; also '
             'preserve the output directory if a single test is run (pass test '
             'name at the end of the command line')
    test_parser.add_argument('tests', nargs='*',
                             help='Test names to run (omit for all)')

    return parser.parse_args(argv)
