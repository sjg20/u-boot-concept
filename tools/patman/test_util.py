# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (c) 2016 Google, Inc
#

from __future__ import print_function

from contextlib import contextmanager
import glob
import os
import sys
import unittest

import patman.command

try:
  from StringIO import StringIO
except ImportError:
  from io import StringIO

PYTHON = 'python%d' % sys.version_info[0]


def RunTestCoverage(prog, filter_fname, exclude_list, build_dir, required=None):
    """Run tests and check that we get 100% coverage

    Args:
        prog: Program to run (with be passed a '-t' argument to run tests
        filter_fname: Normally all *.py files in the program's directory will
            be included. If this is not None, then it is used to filter the
            list so that only filenames that don't contain filter_fname are
            included.
        exclude_list: List of file patterns to exclude from the coverage
            calculation
        build_dir: Build directory, used to locate libfdt.py
        required: List of modules which must be in the coverage report

    Raises:
        ValueError if the code coverage is not 100%
    """
    # This uses the build output from sandbox_spl to get _libfdt.so
    path = os.path.dirname(prog)
    if filter_fname:
        glob_list = glob.glob(os.path.join(path, '*.py'))
        glob_list = [fname for fname in glob_list if filter_fname in fname]
    else:
        glob_list = []
    glob_list += exclude_list
    glob_list += ['*libfdt.py', '*site-packages*', '*dist-packages*']
    test_cmd = 'test' if 'binman.py' in prog else '-t'
    cmd = ('PYTHONPATH=$PYTHONPATH:%s/sandbox_spl/tools %s-coverage run '
           '--omit "%s" %s %s -P1' % (build_dir, PYTHON, ','.join(glob_list),
                                      prog, test_cmd))
    os.system(cmd)
    stdout = command.Output('%s-coverage' % PYTHON, 'report')
    lines = stdout.splitlines()
    if required:
        # Convert '/path/to/name.py' just the module name 'name'
        test_set = set([os.path.splitext(os.path.basename(line.split()[0]))[0]
                        for line in lines if '/etype/' in line])
        missing_list = required
        missing_list.discard('__init__')
        missing_list.difference_update(test_set)
        if missing_list:
            print('Missing tests for %s' % (', '.join(missing_list)))
            print(stdout)
            ok = False

    coverage = lines[-1].split(' ')[-1]
    ok = True
    print(coverage)
    if coverage != '100%':
        print(stdout)
        print("Type '%s-coverage html' to get a report in "
              'htmlcov/index.html' % PYTHON)
        print('Coverage error: %s, but should be 100%%' % coverage)
        ok = False
    if not ok:
        raise ValueError('Test coverage failure')


# Use this to suppress stdout/stderr output:
# with capture_sys_output() as (stdout, stderr)
#   ...do something...
@contextmanager
def capture_sys_output():
    capture_out, capture_err = StringIO(), StringIO()
    old_out, old_err = sys.stdout, sys.stderr
    try:
        sys.stdout, sys.stderr = capture_out, capture_err
        yield capture_out, capture_err
    finally:
        sys.stdout, sys.stderr = old_out, old_err


def ReportResult(toolname:str, test_name: str, result: unittest.TestResult):
    # Remove errors which just indicate a missing test. Since Python v3.5 If an
    # ImportError or AttributeError occurs while traversing name then a
    # synthetic test that raises that error when run will be returned. These
    # errors are included in the errors accumulated by result.errors.
    if test_name:
        errors = []

        for test, err in result.errors:
            if ("has no attribute '%s'" % test_name) not in err:
                errors.append((test, err))
            result.testsRun -= 1
        result.errors = errors

    print(result)
    for test, err in result.errors:
        print(test.id(), err)
    for test, err in result.failures:
        print(err, result.failures)
    if result.skipped:
        print('%d binman test%s SKIPPED:' %
              (len(result.skipped), 's' if len(result.skipped) > 1 else ''))
        for skip_info in result.skipped:
            print('%s: %s' % (skip_info[0], skip_info[1]))
    if result.errors or result.failures:
        print('binman tests FAILED')
        return 1
    return 0
