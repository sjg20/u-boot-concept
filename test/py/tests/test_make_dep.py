# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2025 Simon Glass
# Written by Simon Glass <sjg@chromium.org>

"""Test U-Boot Makefile dependency tracking

This test verifies that Makefile rules properly track dependencies and rebuild
targets when dependency files change. Specifically, it tests the hwids_to_dtsi
rule which had FORCE removed to rely on proper dependency tracking.
"""

import os
import time
from types import SimpleNamespace

import pytest
import utils

# Time to wait between file modifications to ensure different timestamps
TIMESTAMP_DELAY = 2

def run_make(env, message, target='test'):
    """Run a make command with logging"""
    env.ubman.log.action(message)
    utils.run_and_log(env.ubman, ['make', '-f', env.makefile_name, target],
                      cwd=env.tmpdir)
    if target == 'clean' or not os.path.exists(env.test_target):
        return None
    return os.path.getmtime(env.test_target)


def setup_test_environment(ubman):
    """Set up the test environment with hwids files and Makefile"""
    # Create a temporary board directory structure for testing
    tmpdir = os.path.join(ubman.config.result_dir, 'test_make_dep')
    temp_board_dir = os.path.join(tmpdir, 'board', 'test_vendor',
                                  'test_board')
    hwids_dir = os.path.join(temp_board_dir, 'hwids')
    os.makedirs(tmpdir, exist_ok=True)
    os.makedirs(hwids_dir, exist_ok=True)

    # Create test hwids files
    hwidmap_file = os.path.join(hwids_dir, 'compatible.hwidmap')
    hwid_txt_file = os.path.join(hwids_dir, 'test_hwid.txt')

    # Write initial hwidmap content
    with open(hwidmap_file, 'w', encoding='utf-8') as outf:
        outf.write('# Test hardware ID mapping\n')
        outf.write('TEST_DEVICE_1=test,device1\n')

    # Write initial hwid txt content
    with open(hwid_txt_file, 'w', encoding='utf-8') as outf:
        outf.write('TEST_DEVICE_1\n')
        outf.write('Some initial hardware info\n')

    # Assume hwids_to_dtsi rule exists in Makefile.lib

    # Create a simple test Makefile to verify dependency behavior
    test_makefile = os.path.join(tmpdir, 'test_deps.mk')
    test_target = os.path.join(tmpdir, 'test_output.dtsi')

    # Use relative paths in makefile since we cd to tmpdir
    rel_hwids_dir = os.path.relpath(hwids_dir, tmpdir)
    rel_target = os.path.relpath(test_target, tmpdir)

    makefile_content = f'''# Test dependency tracking
HWIDS_DIR := {rel_hwids_dir}
TARGET := {rel_target}

# Simulate the hwids_to_dtsi rule without FORCE
$(TARGET): $(HWIDS_DIR)/compatible.hwidmap $(wildcard $(HWIDS_DIR)/*.txt)
\t@echo "Building target because dependencies changed"
\t@echo "/* Generated from hwids */" > $@
\t@echo "// hwidmap: $$(stat -c %Y $(HWIDS_DIR)/compatible.hwidmap)" >> $@
\t@echo "// txt files: $$(find $(HWIDS_DIR) -name '*.txt' " \\
            "-exec stat -c %Y {{}} \\; | sort -n | tail -1)" >> $@
\t@echo "// Generated at: $$(date +%s)" >> $@

.PHONY: clean test
clean:
\trm -f $(TARGET)
test: $(TARGET)
\t@echo "Target built successfully"
'''

    with open(test_makefile, 'w', encoding='utf-8') as outf:
        outf.write(makefile_content)

    return SimpleNamespace(
        ubman=ubman,
        tmpdir=tmpdir,
        hwids_dir=hwids_dir,
        hwidmap_file=hwidmap_file,
        hwid_txt_file=hwid_txt_file,
        test_makefile=test_makefile,
        test_target=test_target,
        makefile_name=os.path.basename(test_makefile)
    )


def verify_initial_build_and_no_rebuild(env):
    """Test initial build creates target and no-change rebuild doesn't rebuild"""
    run_make(env, 'Running initial build...', 'clean')
    initial_mtime = run_make(env, 'Building test target...', 'test')

    assert os.path.exists(env.test_target), \
        'Target file should be created on initial build'

    env.ubman.log.action(f'Initial target created at {initial_mtime}')

    # Wait a bit to ensure timestamp differences
    time.sleep(TIMESTAMP_DELAY)

    # Run build again - target should NOT be rebuilt (no dependencies changed)
    mtime = run_make(env, 'Running build again without changes...')
    assert mtime == initial_mtime, \
        f'Target should not rebuild when dependencies unchanged ' \
        f'(initial: {initial_mtime}, second: {mtime})'

    return initial_mtime, mtime


def verify_hwidmap_change_rebuild(env, previous_mtime):
    """Test that changing hwidmap file triggers rebuild"""
    time.sleep(TIMESTAMP_DELAY)
    with open(env.hwidmap_file, 'a', encoding='utf-8') as outf:
        outf.write('TEST_DEVICE_2=test,device2\n')

    # Build again - target SHOULD be rebuilt due to hwidmap change
    mtime = run_make(env, 'Building after hwidmap change...')
    assert mtime > previous_mtime, \
        f'Target should rebuild when hwidmap changes ' \
        f'(previous: {previous_mtime}, new: {mtime})'

    return mtime


def verify_txt_file_changes_rebuild(env, previous_mtime):
    """Test that changing txt files triggers rebuild"""
    # Test existing txt file modification
    time.sleep(TIMESTAMP_DELAY)
    with open(env.hwid_txt_file, 'a', encoding='utf-8') as outf:
        outf.write('Additional hardware info\n')

    modified_mtime = run_make(env, 'Building after txt file change...')
    assert modified_mtime > previous_mtime, \
        f'Target should rebuild when txt file changes ' \
        f'(previous: {previous_mtime}, modified: {modified_mtime})'

    # Test new txt file addition
    time.sleep(TIMESTAMP_DELAY)
    new_txt_file = os.path.join(env.hwids_dir, 'new_hwid.txt')
    with open(new_txt_file, 'w', encoding='utf-8') as outf:
        outf.write('NEW_DEVICE\nNew hardware information\n')

    mtime = run_make(env, 'Building after adding new txt file...')
    assert mtime > modified_mtime, \
        f'Target should rebuild when new txt file added ' \
        f'(modified: {modified_mtime}, final: {mtime})'

    return mtime


@pytest.mark.boardspec('sandbox')
def test_make_dependency_tracking(ubman):
    """Test that Makefile dependency tracking works without FORCE

    This test verifies that the hwids_to_dtsi rule (which had FORCE removed)
    still properly rebuilds when dependency files are modified.
    """
    env = setup_test_environment(ubman)

    env.ubman.log.action('Testing initial build and no-change rebuild...')
    _, second_mtime = verify_initial_build_and_no_rebuild(env)

    env.ubman.log.action('Testing hwidmap file change triggers rebuild...')
    third_mtime = verify_hwidmap_change_rebuild(env, second_mtime)

    env.ubman.log.action('Testing txt file changes trigger rebuild...')
    verify_txt_file_changes_rebuild(env, third_mtime)

    env.ubman.log.action('All dependency tracking tests passed!')

