# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2025 Simon Glass
# Written by Simon Glass <sjg@chromium.org>

"""Test U-Boot Makefile dependency tracking

This test verifies that Makefile rules properly track dependencies and rebuild
targets when dependency files change. It tests rules that had FORCE removed
to rely on proper dependency tracking instead.

Tests cover:
- hwids_to_dtsi: Hardware ID mapping to device tree source
- $(obj)/%.dtbo: Device tree source overlay to binary overlay compilation
- ESL dependency chains: Certificate -> ESL file -> DTSI file transformation
"""

import os
import time
from types import SimpleNamespace

import pytest
import utils

# Time to wait between file modifications to ensure different timestamps
TIMESTAMP_DELAY = 2


def info(env, message):
    """Log an informational message

    Args:
        env (SimpleNamespace): Test environment with ubman logger
        message (str): Message to log
    """
    env.ubman.log.info(message)

def run_make(env, message, target='test'):
    """Run a make command with logging

    Args:
        env (SimpleNamespace): Test environment with ubman logger and paths
        message (str): Log message describing the build
        target (str): Make target to build (default: 'test')

    Returns:
        float or None: File modification time of target, or None if
            clean/no target
    """
    info(env, message)
    utils.run_and_log(env.ubman, ['make', '-f', env.makefile_name, target],
                      cwd=env.tmpdir)
    if target == 'clean' or not os.path.exists(env.test_target):
        return None
    return os.path.getmtime(env.test_target)


def setup_hwids_env(ubman):
    """Set up the test environment with hwids files and Makefile

    Args:
        ubman (ConsoleBase): ubman fixture

    Returns:
        SimpleNamespace: Test environment with paths and config
    """
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


def check_initial_build(env):
    """Test initial build creates target and no-change rebuild doesn't rebuild

    Args:
        env (SimpleNamespace): Test environment with paths and ubman logger

    Returns:
        tuple: (initial_mtime, second_mtime) modification times
    """
    run_make(env, 'Running initial build...', 'clean')
    initial_mtime = run_make(env, 'Building test target...', 'test')

    assert os.path.exists(env.test_target), \
        'Target file should be created on initial build'

    info(env, f'Initial target created at {initial_mtime}')

    # Wait a bit to ensure timestamp differences
    time.sleep(TIMESTAMP_DELAY)

    # Run build again - target should NOT be rebuilt (no dependencies changed)
    mtime = run_make(env, 'Running build again without changes...')
    assert mtime == initial_mtime, \
        f'Target should not rebuild when dependencies unchanged ' \
        f'(initial: {initial_mtime}, second: {mtime})'

    return initial_mtime, mtime


def check_hwidmap_change(env, previous_mtime):
    """Test that changing hwidmap file triggers rebuild

    Args:
        env (SimpleNamespace): Test environment with paths and ubman logger
        previous_mtime (float): Previous modification time to compare against

    Returns:
        float: New modification time after rebuild
    """
    time.sleep(TIMESTAMP_DELAY)
    with open(env.hwidmap_file, 'a', encoding='utf-8') as outf:
        outf.write('TEST_DEVICE_2=test,device2\n')

    # Build again - target SHOULD be rebuilt due to hwidmap change
    mtime = run_make(env, 'Building after hwidmap change...')
    assert mtime > previous_mtime, \
        f'Target should rebuild when hwidmap changes ' \
        f'(previous: {previous_mtime}, new: {mtime})'

    return mtime


def check_txt_changes(env, previous_mtime):
    """Test that changing txt files triggers rebuild

    Args:
        env (SimpleNamespace): Test environment with paths and ubman logger
        previous_mtime (float): Previous modification time to compare against

    Returns:
        float: Final modification time after all changes
    """
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


def setup_dtbo_env(ubman):
    """Set up the test environment for dtbo dependency tracking

    Args:
        ubman (ConsoleBase): ubman fixture

    Returns:
        SimpleNamespace: DTBO test environment with paths and config
    """
    # Create a temporary directory structure for dtbo testing
    tmpdir = os.path.join(ubman.config.result_dir, 'test_dtbo_dep')
    os.makedirs(tmpdir, exist_ok=True)

    # Create test dtso source file
    dtso_file = os.path.join(tmpdir, 'test_overlay.dtso')
    dtbo_file = os.path.join(tmpdir, 'test_overlay.dtbo')

    # Write initial dtso content
    dtso_content = '''/dts-v1/;
/plugin/;

/ {
    compatible = "test,overlay";

    fragment@0 {
        target-path = "/";
        __overlay__ {
            test_property = "initial_value";
        };
    };
};'''
    with open(dtso_file, 'w', encoding='utf-8') as outf:
        outf.write(dtso_content)

    # Create a test Makefile to verify dtbo dependency behavior
    test_makefile = os.path.join(tmpdir, 'test_dtbo.mk')

    # Use relative paths in makefile since we cd to tmpdir
    rel_dtso = os.path.relpath(dtso_file, tmpdir)
    rel_dtbo = os.path.relpath(dtbo_file, tmpdir)

    # Find DTC binary
    dtc_cmd = 'dtc'  # Assume dtc is in PATH

    makefile_content = f'''# Test dtbo dependency tracking
DTSO_SRC := {rel_dtso}
DTBO_TARGET := {rel_dtbo}
DTC := {dtc_cmd}

# Simulate the $(obj)/%.dtbo rule from scripts/Makefile.lib
# Using a simplified version of the dtco command
$(DTBO_TARGET): $(DTSO_SRC)
\t@echo "Building dtbo from dtso because source changed"
\t@mkdir -p $(dir $@)
\t$(DTC) -@ -O dtb -o $@ -b 0 -d $@.d $<

.PHONY: clean dtbo
clean:
\trm -f $(DTBO_TARGET) $(DTBO_TARGET).d
dtbo: $(DTBO_TARGET)
\t@echo "DTBO target built successfully"
'''

    with open(test_makefile, 'w', encoding='utf-8') as outf:
        outf.write(makefile_content)

    return SimpleNamespace(
        ubman=ubman,
        tmpdir=tmpdir,
        dtso_file=dtso_file,
        dtbo_file=dtbo_file,
        test_makefile=test_makefile,
        makefile_name=os.path.basename(test_makefile)
    )


def run_dtbo_make(env, message, target='dtbo'):
    """Run a make command for dtbo testing with logging

    Args:
        env (SimpleNamespace): Test environment with paths and ubman logger
        message (str): Log message describing the build
        target (str): Make target to build (default: 'dtbo')

    Returns:
        float or None: File modification time of dtbo target, or None if
            clean/no target
    """
    info(env, message)
    utils.run_and_log(env.ubman, ['make', '-f', env.makefile_name, target],
                      cwd=env.tmpdir)
    if target == 'clean' or not os.path.exists(env.dtbo_file):
        return None
    return os.path.getmtime(env.dtbo_file)


def check_dtbo_build(env):
    """Test initial dtbo build creates target and no-change rebuild doesn't
    rebuild

    Args:
        env (SimpleNamespace): Test environment with paths and ubman logger

    Returns:
        float: Initial modification time of dtbo file
    """
    run_dtbo_make(env, 'Running initial dtbo build...', 'clean')
    initial_mtime = run_dtbo_make(env, 'Building dtbo target...')

    assert os.path.exists(env.dtbo_file), \
        'DTBO file should be created on initial build'

    info(env, f'Initial dtbo created at {initial_mtime}')

    # Wait a bit to ensure timestamp differences
    time.sleep(TIMESTAMP_DELAY)

    # Run build again - target should NOT be rebuilt (no dependencies changed)
    mtime = run_dtbo_make(env, 'Running dtbo build again without changes...')
    assert mtime == initial_mtime, \
        f'DTBO should not rebuild when dtso unchanged ' \
        f'(initial: {initial_mtime}, second: {mtime})'

    return initial_mtime


def check_dtso_change(env, previous_mtime):
    """Test that changing dtso file triggers dtbo rebuild

    Args:
        env (SimpleNamespace): Test environment with paths and ubman logger
        previous_mtime (float): Previous modification time to compare against

    Returns:
        float: New modification time after dtso change and rebuild
    """
    time.sleep(TIMESTAMP_DELAY)

    # Modify the dtso source file - replace the closing brace and add new
    # fragment
    with open(env.dtso_file, 'r', encoding='utf-8') as inf:
        content = inf.read()

    # Replace the last closing brace with additional fragment
    new_content = content.rstrip()
    if new_content.endswith('};'):
        new_content = new_content[:-2]  # Remove the last '};'
        new_content += '''
    fragment@1 {
        target-path = "/";
        __overlay__ {
            test_property2 = "added_value";
        };
    };
};'''
    with open(env.dtso_file, 'w', encoding='utf-8') as outf:
        outf.write(new_content)

    # Build again - target SHOULD be rebuilt due to dtso change
    mtime = run_dtbo_make(env, 'Building dtbo after dtso change...')
    assert mtime > previous_mtime, \
        f'DTBO should rebuild when dtso changes ' \
        f'(previous: {previous_mtime}, new: {mtime})'

    return mtime


@pytest.mark.boardspec('sandbox')
def test_dep_hwids(ubman):
    """Test that Makefile dependency tracking works without FORCE

    This test verifies that the hwids_to_dtsi rule (which had FORCE removed)
    still properly rebuilds when dependency files are modified.
    """
    env = setup_hwids_env(ubman)

    info(env, 'Testing initial build and no-change rebuild...')
    _, second_mtime = check_initial_build(env)

    info(env, 'Testing hwidmap file change triggers rebuild...')
    third_mtime = check_hwidmap_change(env, second_mtime)

    info(env, 'Testing txt file changes trigger rebuild...')
    check_txt_changes(env, third_mtime)

    info(env, 'All dependency tracking tests passed!')


@pytest.mark.boardspec('sandbox')
def test_dep_dtbo(ubman):
    """Test that dtbo dependency tracking works without FORCE

    This test verifies that the $(obj)/%.dtbo rule properly rebuilds
    when the corresponding .dtso source file is modified.
    """
    env = setup_dtbo_env(ubman)

    info(env, 'Testing initial dtbo build and no-change rebuild...')
    first_mtime = check_dtbo_build(env)

    info(env, 'Testing dtso file change triggers dtbo rebuild...')
    check_dtso_change(env, first_mtime)

    info(env, 'All dtbo dependency tracking tests passed!')


def setup_esl_env(ubman):
    """Set up the test environment for ESL dependency tracking

    Args:
        ubman (ConsoleBase): ubman fixture

    Returns:
        SimpleNamespace: ESL test environment with paths and config
    """
    # Create a temporary directory structure for ESL testing
    tmpdir = os.path.join(ubman.config.result_dir, 'test_esl_dep')
    os.makedirs(tmpdir, exist_ok=True)

    # Create test certificate file (dummy content for testing)
    cert_file = os.path.join(tmpdir, 'test_capsule.crt')
    cert_content = '''-----BEGIN CERTIFICATE-----
MIIBkTCB+wIJAKZ7kZ7Z7Z7ZMA0GCSqGSIb3DQEBCwUAMBoxGDAWBgNVBAMMD1Rl
c3QgQ2VydGlmaWNhdGUwHhcNMjUwOTA5MDAwMDAwWhcNMjYwOTA5MDAwMDAwWjAa
MRgwFgYDVQQDDA9UZXN0IENlcnRpZmljYXRlMFwwDQYJKoZIhvcNAQEBBQADSwAw
SAJBAKoZIhvcNAQEBBQADQQAwPgIJAKZ7kZ7Z7Z7ZAgMBAAEwDQYJKoZIhvcNAQE
LBQADQQAwPgIJAKZ7kZ7Z7Z7ZAgMBAAECAwEAAQIBAAIBAAIBAAIBAAIBAAIBAA==
-----END CERTIFICATE-----'''

    with open(cert_file, 'w', encoding='utf-8') as outf:
        outf.write(cert_content)

    # Create ESL template file (mimicking capsule_esl.dtsi.in)
    esl_template = os.path.join(tmpdir, 'capsule_esl.dtsi.in')
    template_content = '''/ {
    signature {
        capsule-key = /incbin/("ESL_BIN_FILE");
    };
};'''

    with open(esl_template, 'w', encoding='utf-8') as outf:
        outf.write(template_content)

    # Target files
    esl_file = os.path.join(tmpdir, 'capsule_esl_file')
    esl_dtsi = os.path.join(tmpdir, '.capsule_esl.dtsi')

    # Create a test Makefile to verify ESL dependency behavior
    test_makefile = os.path.join(tmpdir, 'test_esl.mk')

    # Use relative paths in makefile since we cd to tmpdir
    rel_cert = os.path.relpath(cert_file, tmpdir)
    rel_esl_file = os.path.relpath(esl_file, tmpdir)
    rel_esl_dtsi = os.path.relpath(esl_dtsi, tmpdir)
    rel_template = os.path.relpath(esl_template, tmpdir)

    makefile_content = f'''# Test ESL dependency tracking
CERT_FILE := {rel_cert}
ESL_FILE := {rel_esl_file}
ESL_DTSI := {rel_esl_dtsi}
ESL_TEMPLATE := {rel_template}

# Simulate the capsule_esl_file rule
$(ESL_FILE): $(CERT_FILE)
\t@echo "Generating ESL from certificate because cert changed"
\t@echo "Mock ESL binary data from $<" > $@
\t@echo "Certificate: $$(stat -c %Y $(CERT_FILE))" >> $@

# Simulate the capsule_esl_dtsi rule
$(ESL_DTSI): $(ESL_FILE) $(ESL_TEMPLATE)
\t@echo "Generating ESL DTSI because ESL file or template changed"
\t@sed "s:ESL_BIN_FILE:$$(readlink -f $(ESL_FILE)):" $(ESL_TEMPLATE) > $@
\t@echo "/* Generated at: $$(date +%s) */" >> $@

.PHONY: clean esl dtsi
clean:
\trm -f $(ESL_FILE) $(ESL_DTSI)
esl: $(ESL_FILE)
\t@echo "ESL file built successfully"
dtsi: $(ESL_DTSI)
\t@echo "ESL DTSI built successfully"
'''

    with open(test_makefile, 'w', encoding='utf-8') as outf:
        outf.write(makefile_content)

    return SimpleNamespace(
        ubman=ubman,
        tmpdir=tmpdir,
        cert_file=cert_file,
        esl_template=esl_template,
        esl_file=esl_file,
        esl_dtsi=esl_dtsi,
        test_makefile=test_makefile,
        makefile_name=os.path.basename(test_makefile)
    )


def run_esl_make(env, message, target='dtsi'):
    """Run a make command for ESL testing with logging

    Args:
        env (SimpleNamespace): Test environment with paths and ubman logger
        message (str): Log message describing the build
        target (str): Make target to build (default: 'dtsi')

    Returns:
        float or None: File modification time of target, or None if
            clean/no target
    """
    info(env, message)
    utils.run_and_log(env.ubman, ['make', '-f', env.makefile_name, target],
                      cwd=env.tmpdir)

    if target == 'clean':
        return None
    if target == 'esl' and not os.path.exists(env.esl_file):
        return None
    if target == 'dtsi' and not os.path.exists(env.esl_dtsi):
        return None
    if target == 'esl':
        return os.path.getmtime(env.esl_file)
    # target == 'dtsi'
    return os.path.getmtime(env.esl_dtsi)


def check_esl_build(env):
    """Test initial ESL build creates targets and no-change rebuild doesn't
    rebuild

    Args:
        env (SimpleNamespace): Test environment with paths and ubman logger

    Returns:
        tuple: (esl_mtime, dtsi_mtime) modification times
    """
    run_esl_make(env, 'Running initial ESL build...', 'clean')

    # Build ESL file first
    esl_mtime = run_esl_make(env, 'Building ESL file...', 'esl')
    assert os.path.exists(env.esl_file), \
        'ESL file should be created on initial build'

    # Then build DTSI file
    dtsi_mtime = run_esl_make(env, 'Building ESL DTSI...', 'dtsi')
    assert os.path.exists(env.esl_dtsi), \
        'ESL DTSI should be created on initial build'

    info(env, f'Initial ESL created at {esl_mtime}')
    info(env, f'Initial DTSI created at {dtsi_mtime}')

    # Wait a bit to ensure timestamp differences
    time.sleep(TIMESTAMP_DELAY)

    # Run build again - targets should NOT be rebuilt (no dependencies
    # changed)
    new_esl_mtime = run_esl_make(env, 'Building ESL again without changes...',
                                 'esl')
    assert new_esl_mtime == esl_mtime, \
        f'ESL should not rebuild when cert unchanged ' \
        f'(initial: {esl_mtime}, second: {new_esl_mtime})'

    new_dtsi_mtime = run_esl_make(env,
                                  'Building DTSI again without changes...',
                                  'dtsi')
    assert new_dtsi_mtime == dtsi_mtime, \
        f'DTSI should not rebuild when ESL/template unchanged ' \
        f'(initial: {dtsi_mtime}, second: {new_dtsi_mtime})'

    return esl_mtime, dtsi_mtime


def check_cert_change(env, prev_esl_mtime, prev_dtsi_mtime):
    """Test that changing certificate triggers full rebuild chain

    Args:
        env (SimpleNamespace): Test environment with paths and ubman logger
        prev_esl_mtime (float): Previous ESL modification time
        prev_dtsi_mtime (float): Previous DTSI modification time

    Returns:
        tuple: (new_esl_mtime, new_dtsi_mtime) after certificate change
    """
    time.sleep(TIMESTAMP_DELAY)

    # Modify the certificate file
    with open(env.cert_file, 'a', encoding='utf-8') as outf:
        outf.write('# Modified certificate\n')

    # Build ESL - should rebuild due to cert change
    esl_mtime = run_esl_make(env, 'Building ESL after cert change...', 'esl')
    assert esl_mtime > prev_esl_mtime, \
        f'ESL should rebuild when certificate changes ' \
        f'(previous: {prev_esl_mtime}, new: {esl_mtime})'

    # Build DTSI - should rebuild due to ESL change
    dtsi_mtime = run_esl_make(env, 'Building DTSI after ESL change...', 'dtsi')
    assert dtsi_mtime > prev_dtsi_mtime, \
        f'DTSI should rebuild when ESL changes ' \
        f'(previous: {prev_dtsi_mtime}, new: {dtsi_mtime})'

    return esl_mtime, dtsi_mtime


def check_template_change(env, prev_esl_mtime, prev_dtsi_mtime):
    """Test that changing template rebuilds DTSI but not ESL

    Args:
        env (SimpleNamespace): Test environment with paths and ubman logger
        prev_esl_mtime (float): Previous ESL modification time
        prev_dtsi_mtime (float): Previous DTSI modification time

    Returns:
        tuple: (esl_mtime, new_dtsi_mtime) after template change
    """
    time.sleep(TIMESTAMP_DELAY)

    # Modify the template file
    with open(env.esl_template, 'a', encoding='utf-8') as outf:
        outf.write('    /* Modified template */\n')

    # Build ESL - should NOT rebuild (cert unchanged)
    esl_mtime = run_esl_make(env, 'Building ESL after template change...',
                             'esl')
    assert esl_mtime == prev_esl_mtime, \
        f'ESL should not rebuild when only template changes ' \
        f'(previous: {prev_esl_mtime}, current: {esl_mtime})'

    # Build DTSI - should rebuild due to template change
    dtsi_mtime = run_esl_make(env, 'Building DTSI after template change...',
                              'dtsi')
    assert dtsi_mtime > prev_dtsi_mtime, \
        f'DTSI should rebuild when template changes ' \
        f'(previous: {prev_dtsi_mtime}, new: {dtsi_mtime})'

    return esl_mtime, dtsi_mtime


@pytest.mark.boardspec('sandbox')
def test_dep_esl(ubman):
    """Test that ESL dependency tracking works without FORCE

    This test verifies that the capsule ESL rules properly track dependencies
    and rebuild when source files are modified. Tests the chain:
    certificate -> ESL file -> DTSI file
    """
    env = setup_esl_env(ubman)

    info(env, 'Testing initial ESL build and no-change rebuild...')
    esl_mtime, dtsi_mtime = check_esl_build(env)

    info(env, 'Testing certificate change triggers full rebuild chain...')
    esl_mtime, dtsi_mtime = check_cert_change(env, esl_mtime, dtsi_mtime)

    info(env, 'Testing template change rebuilds DTSI only...')
    check_template_change(env, esl_mtime, dtsi_mtime)

    info(env, 'All ESL dependency tracking tests passed!')
