# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2025, Canonical Ltd.

"""Test U-Boot library functionality"""

import os
import subprocess
import pytest
import utils

def check_output(out):
    """Check output from the ulib test"""
    assert 'Hello, world from ub_printf' in out
    assert '- U-Boot' in out
    assert 'Uses libc printf before ulib_init' in out
    assert 'another printf()' in out

@pytest.mark.buildconfigspec("ulib")
def test_ulib_shared(ubman):
    """Test the ulib shared library test program"""

    build = ubman.config.build_dir
    prog = os.path.join(build, 'test', 'ulib', 'ulib_test')

    # Skip test if ulib_test doesn't exist (clang)
    if not os.path.exists(prog):
        pytest.skip('ulib_test not found - library build may be disabled')

    out = utils.run_and_log(ubman, [prog], cwd=build)
    check_output(out)
    assert 'dynamically linked' in out

@pytest.mark.boardspec('sandbox')
def test_ulib_static(ubman):
    """Test the ulib static library test program"""

    build = ubman.config.build_dir
    prog = os.path.join(build, 'test', 'ulib', 'ulib_test_static')

    # Skip test if ulib_test_static doesn't exist (clang)
    if not os.path.exists(prog):
        pytest.skip('ulib_test_static not found - library build may be disabled')

    out = utils.run_and_log(ubman, [prog])
    check_output(out)
    assert 'statically linked' in out

def check_demo_output(ubman, out):
    """Check output from the ulib demo programs exactly line by line"""
    lines = out.split('\n')

    # Read the actual system version from /proc/version
    with open('/proc/version', 'r', encoding='utf-8') as f:
        proc_version = f.read().strip()

    expected = [
        'U-Boot Library Demo Helper\r',
        '==========================\r',
        'System version:helper: Adding 42 + 13 = 55\r',
        '=================================\r',
        'Demo complete\r',
        f'U-Boot version: {ubman.u_boot_version_string}',
        '',
        f'  {proc_version}',
        '',
        'Read 1 line(s) using U-Boot library functions.',
        'Helper function result: 55',
        ''
    ]

    assert len(lines) == len(expected), \
        f"Expected {len(expected)} lines, got {len(lines)}"

    for i, expected in enumerate(expected):
        # Exact match for all other lines
        assert lines[i] == expected, \
            f"Line {i}: expected '{expected}', got '{lines[i]}'"

def check_rust_demo_output(_ubman, out):
    """Check output from the Rust ulib demo programs exactly line by line"""
    lines = out.split('\n')

    # Read the actual system version from /proc/version
    with open('/proc/version', 'r', encoding='utf-8') as f:
        proc_version = f.read().strip()

    # Check individual parts of the output
    assert len(lines) == 13, f"Expected 13 lines, got {len(lines)}"

    assert lines[0] == 'U-Boot Library Demo Helper\r'
    assert lines[1] == '==========================\r'
    assert lines[2].startswith('U-Boot version: ') and lines[2].endswith('\r')
    assert lines[3] == '\r'
    assert lines[4] == 'System version:\r'
    assert lines[5] == f'  {proc_version}\r'
    assert lines[6] == '\r'
    assert lines[7] == 'Read 1 line(s) using U-Boot library functions.\r'
    assert lines[8] == 'helper: Adding 42 + 13 = 55\r'
    assert lines[9] == 'Helper function result: 55\r'
    assert lines[10] == '=================================\r'
    assert lines[11] == 'Demo complete (hi from rust)\r'
    assert lines[12] == ''

@pytest.mark.boardspec('sandbox')
def test_ulib_demos(ubman):
    """Test both ulib demo programs (dynamic and static)."""

    build = ubman.config.build_dir
    src = ubman.config.source_dir
    examples = os.path.join(src, 'examples', 'ulib')
    test_program = os.path.join(build, 'test', 'ulib', 'ulib_test')

    # Skip test if ulib_test doesn't exist (clang)
    if not os.path.exists(test_program):
        pytest.skip('ulib_test not found - library build may be disabled')

    # Build the demo programs - clean first to ensure fresh build, since this
    # test is run in the source directory
    cmd = ['make', 'clean']
    utils.run_and_log(ubman, cmd, cwd=examples)

    cmd = ['make', f'UBOOT_BUILD={os.path.abspath(build)}', f'srctree={src}']
    utils.run_and_log(ubman, cmd, cwd=examples)

    # Test static demo program
    demo_static = os.path.join(examples, 'demo_static')
    out_static = utils.run_and_log(ubman, [demo_static])
    check_demo_output(ubman, out_static)

    # Test dynamic demo program (with proper LD_LIBRARY_PATH)
    demo = os.path.join(examples, 'demo')
    env = os.environ.copy()
    env['LD_LIBRARY_PATH'] = os.path.abspath(build)
    out_dynamic = utils.run_and_log(ubman, [demo], env=env)
    check_demo_output(ubman, out_dynamic)

@pytest.mark.boardspec('sandbox')
def test_ulib_rust_demos(ubman):
    """Test both Rust ulib demo programs (dynamic and static)."""

    build = ubman.config.build_dir
    src = ubman.config.source_dir
    examples = os.path.join(src, 'examples', 'rust')
    test_program = os.path.join(build, 'test', 'ulib', 'ulib_test')

    # Skip test if ulib_test doesn't exist (clang)
    if not os.path.exists(test_program):
        pytest.skip('ulib_test not found - library build may be disabled')

    # Check if cargo is available
    try:
        subprocess.run(['cargo', '--version'], check=True,
                      capture_output=True, text=True)
    except (subprocess.CalledProcessError, FileNotFoundError):
        pytest.skip('cargo not found - Rust toolchain required')

    # Build the Rust demo programs - clean first to ensure fresh build
    cmd = ['make', 'clean']
    utils.run_and_log(ubman, cmd, cwd=examples)

    cmd = ['make', f'UBOOT_BUILD={os.path.abspath(build)}', f'srctree={src}']
    utils.run_and_log(ubman, cmd, cwd=examples)

    # Test static demo program
    demo_static = os.path.join(examples, 'demo_static')
    out_static = utils.run_and_log(ubman, [demo_static])
    check_rust_demo_output(ubman, out_static)

    # Test dynamic demo program (with proper LD_LIBRARY_PATH)
    demo = os.path.join(examples, 'demo')
    env = os.environ.copy()
    env['LD_LIBRARY_PATH'] = os.path.abspath(build)
    out_dynamic = utils.run_and_log(ubman, [demo], env=env)
    check_rust_demo_output(ubman, out_dynamic)

@pytest.mark.boardspec('sandbox')
def test_ulib_api_header(ubman):
    """Test that the u-boot-api.h header is generated correctly."""

    hdr = os.path.join(ubman.config.build_dir, 'include', 'u-boot-api.h')

    # Skip if header doesn't exist (clang)
    if not os.path.exists(hdr):
        pytest.skip('u-boot-api.h not found - library build may be disabled')

    # Read and verify header content
    with open(hdr, 'r', encoding='utf-8') as inf:
        out = inf.read()

    # Check header guard
    assert '#ifndef __ULIB_API_H' in out
    assert '#define __ULIB_API_H' in out
    assert '#endif /* __ULIB_API_H */' in out

    # Check required includes
    assert '#include <stdarg.h>' in out
    assert '#include <stddef.h>' in out

    # Check for renamed function declarations
    assert 'ub_printf' in out
    assert 'ub_snprintf' in out
    assert 'ub_vprintf' in out

    # Check that functions have proper signatures
    assert 'ub_printf(const char *fmt, ...)' in out
    assert 'ub_snprintf(char *buf, size_t size, const char *fmt, ...)' in out
    assert 'ub_vprintf(const char *fmt, va_list args)' in out
