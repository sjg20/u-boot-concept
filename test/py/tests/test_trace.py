# SPDX-License-Identifier: GPL-2.0
# Copyright 2022 Google LLC
# Written by Simon Glass <sjg@chromium.org>

import os
import pytest
import re

import u_boot_utils as util

# This is needed for Azure, since the default '..' directory is not writeable
TMPDIR = '/tmp/test_trace'

@pytest.mark.slow
@pytest.mark.boardspec('sandbox')
def test_trace(u_boot_console):
    """Test we can build sandbox with trace, collect and process a trace"""
    cons = u_boot_console

    env = dict(os.environb)

    # Enable tracing and disable LTO, to ensure functions are not elided
    env['FTRACE'] = '1'
    env['NO_LTO'] = '1'
    options = ['CONFIG_TRACE=y', 'CONFIG_TRACE_EARLY=y',
               'CONFIG_TRACE_EARLY_SIZE=0x01000000']
    cfgs = [x for opt in options for x in ('-a', opt)]
    #out = util.run_and_log(
        #cons, ['./tools/buildman/buildman', '-m', '--board', 'sandbox',
               #'-o', TMPDIR, '-w', *cfgs], ignore_errors=True, env=env)

    cons.restart_uboot_with_flags([], build_dir=TMPDIR)
    u_boot_console.run_command('trace pause')
    out = u_boot_console.run_command('trace stats')

    # The output is something like this:
    #    251,003 function sites
    #  1,160,283 function calls
    #          0 untracked function calls
    #  1,230,758 traced function calls (341538 dropped due to overflow)
    #         33 maximum observed call depth
    #         15 call depth limit
    #    748,268 calls not traced due to depth
    #  1,230,758 max function calls

    # Get a dict of values from the output
    lines = [line.split(maxsplit=1) for line in out.splitlines() if line]
    vals = {key: val.replace(',', '') for val, key in lines}

    assert int(vals['function sites']) > 100000
    assert int(vals['function calls']) > 200000
    assert int(vals['untracked function calls']) == 0
    assert int(vals['maximum observed call depth']) > 30
    assert (vals['call depth limit'] ==
            cons.config.buildconfig.get('config_trace_call_depth_limit'))
    assert int(vals['calls not traced due to depth']) > 100000

    # Read out the trace data
    addr = 0x02000000
    size = 0x01000000
    out = u_boot_console.run_command(f'trace calls {addr:x} {size:x}')
    print(out)
    fname = os.path.join(TMPDIR, 'trace')
    out = u_boot_console.run_command(
        'host save hostfs - %x %s ${profoffset}' % (addr, fname))

    # Convert it using proftool
    proftool = os.path.join(TMPDIR, 'tools', 'proftool')
    map_fname = os.path.join(TMPDIR, 'System.map')
    trace_dat = os.path.join(TMPDIR, 'trace.dat')
    out = util.run_and_log(
        cons, [proftool, '-t', fname, '-o', trace_dat, '-m', map_fname,
               'dump-ftrace'])

    # Check that trace-cmd can read it
    out = util.run_and_log(cons, ['trace-cmd', 'dump', trace_dat])

    # Tracing meta data in file /tmp/test_trace/trace.dat:
    #    [Initial format]
    #            6        [Version]
    #            0        [Little endian]
    #            4        [Bytes in a long]
    #            4096        [Page size, bytes]
    #    [Header page, 205 bytes]
    #    [Header event, 205 bytes]
    #    [Ftrace format, 3 events]
    #    [Events format, 0 systems]
    #    [Kallsyms, 342244 bytes]
    #    [Trace printk, 0 bytes]
    #    [Saved command lines, 9 bytes]
    #    1 [CPUs with tracing data]
    #    [6 options]
    #    [Flyrecord tracing data]
    #    [Tracing clock]
    #            [local] global counter uptime perf mono mono_raw boot x86-tsc
    assert '[Flyrecord tracing data]' in out
    assert '4096	[Page size, bytes]' in out
    kallsyms = [line.split() for line in out.splitlines() if 'Kallsyms' in line]
    # [['[Kallsyms,', '342244', 'bytes]']]
    val = int(kallsyms[0][1])
    assert val > 50000  # Should be at least 50KB of symbols

    # Check that the trace has something useful
    cmd = f"trace-cmd report {trace_dat} |egrep '(initf_|initr_)'"
    out = util.run_and_log(cons, ['sh', '-c', cmd])

    # Format:
    #      u-boot-1     [000]    60.805596: function:             initf_malloc
    #      u-boot-1     [000]    60.805597: function:             initf_malloc
    #      u-boot-1     [000]    60.805601: function:             initf_bootstage
    #      u-boot-1     [000]    60.805607: function:             initf_bootstage
    lines = [line.replace(':', '').split() for line in out.splitlines()]
    vals = {items[4]: float(items[2]) for items in lines}
    base = None
    max_delta = 0
    for timestamp in vals.values():
        if base:
            max_delta = max(max_delta, timestamp - base)
        else:
            base = timestamp

    # Check for some expected functions
    assert 'initf_malloc' in vals.keys()
    assert 'initr_watchdog' in vals.keys()
    assert 'initr_dm' in vals.keys()

    # All the functions should be executed within five seconds at most
    assert max_delta < 5

    # Generate the funcgraph format
    out = util.run_and_log(
        cons, [proftool, '-t', fname, '-o', trace_dat, '-m', map_fname,
               'dump-ftrace', '-f', 'funcgraph'])

    # Check that the trace has what we expect
    cmd = f'trace-cmd report {trace_dat} |head -n 70'
    out = util.run_and_log(cons, ['sh', '-c', cmd])

    # First look for this:
    #  u-boot-1     [000]   282.101360: funcgraph_entry:        0.004 us   |    initf_malloc();
    # ...
    #  u-boot-1     [000]   282.101369: funcgraph_entry:                   |    initf_bootstage() {
    #  u-boot-1     [000]   282.101369: funcgraph_entry:                   |      bootstage_init() {
    #  u-boot-1     [000]   282.101369: funcgraph_entry:                   |        dlmalloc() {
    # ...
    #  u-boot-1     [000]   282.101375: funcgraph_exit:         0.001 us   |        }
    # Then look for this:
    #  u-boot-1     [000]   282.101375: funcgraph_exit:         0.006 us   |      }
    # Then check for this:
    #  u-boot-1     [000]   282.101375: funcgraph_entry:        0.000 us   |    event_init();

    # Look for initf_bootstage() entry and make sure we see the exit
    re_line = re.compile(r'.*[|](\s*)(\S.*)?([{};])$')
    expected_indent = None
    found_start = False
    found_end = False
    upto = None
    for line in out.splitlines():
        m = re_line.match(line)
        if m:
            indent, func, brace = m.groups()
            if found_end:
                upto = func
                break
            elif func == 'initf_bootstage() ' and brace == '{':
                found_start = True
                expected_indent = indent + '  '
            elif found_start and indent == expected_indent and brace == '}':
                found_end = True

    # The next function after initf_bootstage() exits should be event_init()
    assert upto == 'event_init()'

    # Check flame graph


    assert False
