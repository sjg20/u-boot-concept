#!/usr/bin/python3

# SPDX-License-Identifier: GPL-2.0+
# Copyright 2020 Google LLC

"""Lab manager - configuration and maintenance of a lab"""

import os
import sys
import traceback

our_path = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(our_path, '..'))

import cmdline
import control

def RunLabman(args):
    """Main entry point to labman once arguments are parsed

    Args:
        args: Command line arguments Namespace object
    """
    ret_code = 0

    if not args.debug:
        sys.tracebacklimit = 0

    if args.cmd == 'test':
        if args.test_coverage:
            RunTestCoverage()
        else:
            ret_code = RunTests(args.debug, args.verbosity, args.processes,
                                args.test_preserve_dirs, args.tests,
                                args.toolpath)

    else:
        try:
            ret_code = control.Labman(args)
        except Exception as e:
            print('labman: %s' % e)
            if args.debug:
                print()
                traceback.print_exc()
            ret_code = 1
    return ret_code


if __name__ == "__main__":
    args = cmdline.ParseArgs(sys.argv[1:])

    ret_code = RunLabman(args)
    sys.exit(ret_code)
