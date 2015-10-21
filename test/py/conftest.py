# Copyright (c) 2015 Stephen Warren
# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# SPDX-License-Identifier: GPL-2.0

import atexit
import errno
import os
import os.path
import pexpect
import pytest
from _pytest.runner import runtestprotocol
import ConfigParser
import StringIO
import sys

log = None
console = None

def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc:
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            raise

def pytest_addoption(parser):
    parser.addoption("--build-dir", default=None,
        help="U-Boot build directory (O=)")
    parser.addoption("--result-dir", default=None,
        help="U-Boot test result/tmp directory")
    parser.addoption("--persistent-data-dir", default=None,
        help="U-Boot test persistent generated data directory")
    parser.addoption("--board-type", "--bd", "-B", default="sandbox",
        help="U-Boot board type")
    parser.addoption("--board-identity", "--id", default="na",
        help="U-Boot board identity/instance")
    parser.addoption("--build", default=False, action="store_true",
        help="Compile U-Boot before running tests")

def pytest_configure(config):
    global log
    global console
    global ubconfig

    test_py_dir = os.path.dirname(os.path.abspath(__file__))
    source_dir = os.path.dirname(os.path.dirname(test_py_dir))

    board_type = config.getoption("board_type")
    board_type_fn = board_type.replace("-", "_")

    board_identity = config.getoption("board_identity")
    board_identity_fn = board_identity.replace("-", "_")

    build_dir = config.getoption("build_dir")
    if not build_dir:
        build_dir = source_dir + "/build-" + board_type
    mkdir_p(build_dir)

    result_dir = config.getoption("result_dir")
    if not result_dir:
        result_dir = build_dir
    mkdir_p(result_dir)

    persistent_data_dir = config.getoption("persistent_data_dir")
    if not persistent_data_dir:
        persistent_data_dir = build_dir + "/persistent-data"
    mkdir_p(persistent_data_dir)

    import multiplexed_log
    log = multiplexed_log.Logfile(result_dir + "/test-log.html")

    if config.getoption("build"):
        if build_dir != source_dir:
            o_opt = "O=%s" % build_dir
        else:
            o_opt = ""
        cmds = (
            ["make", o_opt, "-s", board_type + "_defconfig"],
            ["make", o_opt, "-s", "-j8"],
        )
        runner = log.get_runner("make", sys.stdout)
        for cmd in cmds:
            runner.run(cmd, cwd=source_dir)
        runner.close()

    class ArbitraryAttrContainer(object):
        pass

    ubconfig = ArbitraryAttrContainer()
    ubconfig.brd = dict()
    ubconfig.env = dict()

    modules = [
        (ubconfig.brd, "uboot_board_" + board_type_fn),
        (ubconfig.env, "uboot_boardenv_" + board_type_fn),
        (ubconfig.env, "uboot_boardenv_" + board_type_fn + "_" +
                                                board_identity_fn),
    ]
    for (sub_config, mod_name) in modules:
        try:
            mod = __import__(mod_name)
        except ImportError:
            continue
        sub_config.update(mod.__dict__)

    ubconfig.buildconfig = dict()

    for conf_file in (".config", "include/autoconf.mk"):
        dot_config = build_dir + "/" + conf_file
        if not os.path.exists(dot_config):
            raise Exception(conf_file + " does not exist; " +
                "try passing --build option?")

        with open(dot_config, "rt") as f:
            ini_str = "[root]\n" + f.read()
            ini_sio = StringIO.StringIO(ini_str)
            parser = ConfigParser.RawConfigParser()
            parser.readfp(ini_sio)
            ubconfig.buildconfig.update(parser.items("root"))

    ubconfig.test_py_dir = test_py_dir
    ubconfig.source_dir = source_dir
    ubconfig.build_dir = build_dir
    ubconfig.result_dir = result_dir
    ubconfig.persistent_data_dir = persistent_data_dir
    ubconfig.board_type = board_type
    ubconfig.board_identity = board_identity

    env_vars = (
        "board_type",
        "board_identity",
        "source_dir",
        "test_py_dir",
        "build_dir",
        "result_dir",
        "persistent_data_dir",
    )
    for v in env_vars:
        os.environ["UBOOT_" + v.upper()] = getattr(ubconfig, v)

    if board_type == "sandbox":
        import uboot_console_sandbox
        console = uboot_console_sandbox.ConsoleSandbox(log, ubconfig)
    else:
        import uboot_console_exec_attach
        console = uboot_console_exec_attach.ConsoleExecAttach(log, ubconfig)

def pytest_generate_tests(metafunc):
    subconfigs = {
        "brd": console.config.brd,
        "env": console.config.env,
    }
    for fn in metafunc.fixturenames:
        parts = fn.split("__")
        if len(parts) < 2:
            continue
        if parts[0] not in subconfigs:
            continue
        subconfig = subconfigs[parts[0]]
        vals = []
        val = subconfig.get(fn, [])
        if val:
            vals = (val, )
        else:
            vals = subconfig.get(fn + "s", [])
        metafunc.parametrize(fn, vals)

@pytest.fixture(scope="session")
def uboot_console(request):
    return console

tests_not_run = set()
tests_failed = set()
tests_skipped = set()
tests_passed = set()

def pytest_itemcollected(item):
    tests_not_run.add(item.name)

def cleanup():
    if console:
        console.close()
    if log:
        log.status_pass("%d passed" % len(tests_passed))
        if tests_skipped:
            log.status_skipped("%d skipped" % len(tests_skipped))
            for test in tests_skipped:
                log.status_skipped("... " + test)
        if tests_failed:
            log.status_fail("%d failed" % len(tests_failed))
            for test in tests_failed:
                log.status_fail("... " + test)
        if tests_not_run:
            log.status_fail("%d not run" % len(tests_not_run))
            for test in tests_not_run:
                log.status_fail("... " + test)
        log.close()
atexit.register(cleanup)

def setup_boardspec(item):
    mark = item.get_marker("boardspec")
    if not mark:
        return
    required_boards = []
    for board in mark.args:
        if board.startswith("!"):
            if ubconfig.board_type == board[1:]:
                pytest.skip("board not supported")
                return
        else:
            required_boards.append(board)
    if required_boards and ubconfig.board_type not in required_boards:
        pytest.skip("board not supported")

def setup_buildconfigspec(item):
    mark = item.get_marker("buildconfigspec")
    if not mark:
        return
    for option in mark.args:
        if not ubconfig.buildconfig.get("config_" + option.lower(), None):
            pytest.skip(".config feature not enabled")

def pytest_runtest_setup(item):
    log.start_section(item.name)
    setup_boardspec(item)
    setup_buildconfigspec(item)

def pytest_runtest_protocol(item, nextitem):
    reports = runtestprotocol(item, nextitem=nextitem)
    failed = None
    skipped = None
    for report in reports:
        if report.outcome == "failed":
            failed = report
            break
        if report.outcome == "skipped":
            if not skipped:
                skipped = report

    if failed:
        tests_failed.add(item.name)
    elif skipped:
        tests_skipped.add(item.name)
    else:
        tests_passed.add(item.name)
    tests_not_run.remove(item.name)

    try:
        if failed:
            msg = "FAILED:\n" + str(failed.longrepr)
            log.status_fail(msg)
        elif skipped:
            msg = "SKIPPED:\n" + str(skipped.longrepr)
            log.status_skipped(msg)
        else:
            log.status_pass("OK")
    except:
        # If something went wrong with logging, it's better to let the test
        # process continue, which may report other exceptions that triggered
        # the logging issue (e.g. console.log wasn't created). Hence, just
        # squash the exception. If the test setup failed due to e.g. syntax
        # error somewhere else, this won't be seen. However, once that issue
        # is fixed, if this exception still exists, it will then be logged as
        # part of the test's stdout.
        import traceback
        print "Exception occurred while logging runtest status:"
        traceback.print_exc()
        # FIXME: Can we force a test failure here?

    log.end_section(item.name)

    if failed:
        console.cleanup_spawn()

    return reports
