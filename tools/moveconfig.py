#!/usr/bin/env python
#
# Author: Masahiro Yamada <yamada.m@jp.panasonic.com>
#
# SPDX-License-Identifier:	GPL-2.0+
#

"""
Move the specified config option to Kconfig
"""

import errno
import fnmatch
import multiprocessing
import optparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

SHOW_GNU_MAKE = 'scripts/show-gnu-make'
SLEEP_TIME=0.03

CROSS_COMPILE = {
    'aarch64': '/opt/linaro/gcc-linaro-aarch64-linux-gnu-4.8-2014.04_linux/bin/aarch64-linux-gnu-',
    'arm': 'arm-linux-gnueabi-',
    'avr32': '/home/sglass/.buildman-toolchains/gcc-4.2.4-nolibc/avr32-linux/bin/avr32-linux-',
    'blackfin': '/home/sglass/.buildman-toolchains/gcc-4.6.3-nolibc/bfin-uclinux/bin/bfin-uclinux-',
    'm68k': '/home/sglass/.buildman-toolchains/gcc-4.6.3-nolibc/m68k-linux/bin/m68k-linux-',
    'microblaze': '/opt/microblaze/lin64-microblazeel-unknown-linux-gnu_14.3_early/bin/microblazeel-unknown-linux-gnu-',
    'mips': '/home/sglass/.buildman-toolchains/gcc-4.6.3-nolibc/mips-linux/bin/mips-linux-',
    'nds32': '/opt/nds32le-linux-glibc-v1f/bin/nds32le-linux-',
    'nios2': '/opt/nios2/bin/nios2-linux-',
    'openrisc': '/home/sglass/.buildman-toolchains/gcc-4.5.1-nolibc/or32-linux/bin/or32-linux-',
    'powerpc': '~/.buildman-toolchains/gcc-4.6.3-nolibc/powerpc-linux/bin/powerpc-linux-',
    'sh': '/home/sglass/.buildman-toolchains/gcc-4.6.3-nolibc/sh4-linux/bin/sh4-linux-',
    'sparc': '/home/sglass/.buildman-toolchains/gcc-4.6.3-nolibc/sparc-linux/bin/sparc-linux-',
    'x86': '/opt/i386-linux/bin/i386-linux-'
}

STATE_IDLE = 0
STATE_DEFCONFIG = 1
STATE_SILENTOLDCONFIG = 2

### helper functions ###
def get_devnull():
    """Get the file object of '/dev/null' device."""
    try:
        devnull = subprocess.DEVNULL # py3k
    except AttributeError:
        devnull = open(os.devnull, 'wb')
    return devnull

def check_top_directory():
    """Exit if we are not at the top of source directory."""
    for f in ('README', 'Licenses'):
        if not os.path.exists(f):
            sys.exit('Please run at the top of source directory.')

def get_make_cmd():
    """Get the command name of GNU Make."""
    process = subprocess.Popen([SHOW_GNU_MAKE], stdout=subprocess.PIPE)
    ret = process.communicate()
    if process.returncode:
        sys.exit('GNU Make not found')
    return ret[0].rstrip()

def cleanup_header(header_path, patterns):
    with open(header_path) as f:
        lines = f.readlines()

    matched = []
    for i, line in enumerate(lines):
        for pattern in patterns:
            m = pattern.search(line)
            if m:
                print '%s: %s: %s' % (header_path, i + 1, line),
                matched.append(i)
                break

    if not matched:
        return

    with open(header_path, 'w') as f:
        for i, line in enumerate(lines):
            if not i in matched:
                f.write(line)

def cleanup_headers(config):
    while True:
        choice = raw_input('Clean up %s in headers? [y/n]: ' % config).lower()
        print choice
        if choice == 'y' or choice == 'n':
            break

    if choice != 'y':
        return

    patterns = []
    patterns.append(re.compile(r'#\s*define\s+%s\W' % config))
    patterns.append(re.compile(r'#\s*undef\s+%s\W' % config))

    for (dirpath, dirnames, filenames) in os.walk('include'):
        for filename in filenames:
            if not fnmatch.fnmatch(filename, '*~'):
                cleanup_header(os.path.join(dirpath, filename), patterns)

### classes ###
class KconfigParser:

    """A parser of .config file.

    Each line of the output should have the form of:
    Status, Arch, CPU, SoC, Vendor, Board, Target, Options, Maintainers
    Most of them are extracted from .config file.
    MAINTAINERS files are also consulted for Status and Maintainers fields.
    """

    re_arch = re.compile(r'CONFIG_SYS_ARCH="(.*)"')
    re_cpu = re.compile(r'CONFIG_SYS_CPU="(.*)"')
    PREFIX = { '.': '+', 'spl': 'S', 'tpl': 'T' }

    def __init__(self, build_dir, config_attr):
        """Create a new .config perser.

        Arguments:
          build_dir: Build directory where .config is located
          output: File object which the result is written to
        """
        self.build_dir = build_dir
        self.config = config_attr['config']
        self.type = config_attr['type']
        self.default = config_attr['default']
        if config_attr['no_spl_support'] == 'y':
            self.no_spl_support = True
        else:
            self.no_spl_support = False
        self.re = re.compile(r'%s=(.*)' % self.config)

    def get_cross_compile(self):
        """Parse .config file and return CROSS_COMPILE

        Arguments:
          defconfig: Board (defconfig) name
        """
        arch = ''
        cpu = ''
        dotconfig = os.path.join(self.build_dir, '.config')
        for line in open(dotconfig):
            m = self.re_arch.match(line)
            if m:
                arch = m.group(1)
                continue
            m = self.re_cpu.match(line)
            if m:
                cpu = m.group(1)

        assert arch, 'Error: arch is not defined in %s' % defconfig

        # fix-up for aarch64
        if arch == 'arm' and cpu == 'armv8':
            arch = 'aarch64'

        return CROSS_COMPILE.get(arch, '')

    def update_defconfig(self, defconfig):
        values = []
        output_lines = []
        prefixes = {}

        if self.no_spl_support:
            images = ('.')
        else:
            images = ('.', 'spl', 'tpl')


        for img in images:
            autoconf = os.path.join(self.build_dir, img, 'include',
                                    'autoconf.mk')

            if not os.path.exists(autoconf):
                values.append('')
                continue

            if self.type == 'bool':
                value = 'n'
            else:
                value = '(undef)'

            for line in open(autoconf):
                m = self.re.match(line)
                if m:
                    value = m.group(1)
                    break

            if value == self.default:
                value = '(default)'

            values.append(value)
            os.remove(autoconf)

            if value == '(undef)' or value == '(default)':
                continue

            if self.type == 'bool' and value == 'n':
                output_line = '# %s is not set' % (self.config)
            else:
                output_line = '%s=%s' % (self.config, value)
            if output_line in output_lines:
                prefixes[output_line] += self.PREFIX[img]
            else:
                output_lines.append(output_line)
                prefixes[output_line] = self.PREFIX[img]

        output = defconfig[:-len('_defconfig')].ljust(37) + ': '
        for value in values:
            output += value.ljust(12)

        print output.strip()

        with open(os.path.join('configs', defconfig), 'a') as f:
            for line in output_lines:
                if prefixes[line] != '+':
                    line = prefixes[line] + ':' + line
                f.write(line + '\n')

class Slot:

    """A slot to store a subprocess.

    Each instance of this class handles one subprocess.
    This class is useful to control multiple processes
    for faster processing.
    """

    def __init__(self, config_attr, devnull, make_cmd):
        """Create a new slot.

        Arguments:
          output: File object which the result is written to
        """
        self.build_dir = tempfile.mkdtemp()
        self.devnull = devnull
        self.make_cmd = (make_cmd, 'O=' + self.build_dir)
        self.parser = KconfigParser(self.build_dir, config_attr)
        self.state = STATE_IDLE

    def __del__(self):
        """Delete the working directory"""
        if self.state != STATE_IDLE:
            while self.ps.poll() == None:
                pass
        shutil.rmtree(self.build_dir)

    def add(self, defconfig):
        """Add a new subprocess to the slot.

        Fails if the slot is occupied, that is, the current subprocess
        is still running.

        Arguments:
          defconfig: Board (defconfig) name

        Returns:
          Return True on success or False on fail
        """
        if self.state != STATE_IDLE:
            return False
        cmd = list(self.make_cmd)
        cmd.append(defconfig)
        self.ps = subprocess.Popen(cmd, stdout=self.devnull)
        self.defconfig = defconfig
        self.state = STATE_DEFCONFIG
        return True

    def poll(self):
        """Check if the subprocess is running and invoke the .config
        parser if the subprocess is terminated.

        Returns:
          Return True if the subprocess is terminated, False otherwise
        """
        if self.state == STATE_IDLE:
            return True

        if self.ps.poll() == None:
            return False

        if self.ps.poll() != 0:
            sys.exit("failed to process '%s'" % self.defconfig)

        if self.state == STATE_SILENTOLDCONFIG:
            self.parser.update_defconfig(self.defconfig)
            self.state = STATE_IDLE
            return True

        cross_compile = self.parser.get_cross_compile()
        cmd = list(self.make_cmd)
        if cross_compile:
            cmd.append('CROSS_COMPILE=%s' % cross_compile)
        cmd.append('silentoldconfig')
        self.ps = subprocess.Popen(cmd, stdout=self.devnull)
        self.state = STATE_SILENTOLDCONFIG
        return False

class Slots:

    """Controller of the array of subprocess slots."""

    def __init__(self, config_attr, jobs):
        """Create a new slots controller.

        Arguments:
          jobs: A number of slots to instantiate
        """
        self.slots = []
        devnull = get_devnull()
        make_cmd = get_make_cmd()
        for i in range(jobs):
            self.slots.append(Slot(config_attr, devnull, make_cmd))

    def add(self, defconfig):
        """Add a new subprocess if a vacant slot is available.

        Arguments:
          defconfig: Board (defconfig) name

        Returns:
          Return True on success or False on fail
        """
        for slot in self.slots:
            if slot.add(defconfig):
                return True
        return False

    def available(self):
        """Check if there is a vacant slot.

        Returns:
          Return True if a vacant slot is found, False if all slots are full
        """
        for slot in self.slots:
            if slot.poll():
                return True
        return False

    def empty(self):
        """Check if all slots are vacant.

        Returns:
          Return True if all slots are vacant, False if at least one slot
          is running
        """
        ret = True
        for slot in self.slots:
            if not slot.poll():
                ret = False
        return ret

def move_config(config_attr, jobs=1):
    check_top_directory()
    print 'Moving %s (type: %s, default: %s, no_spl: %s) ...  (jobs: %d)' % (
        config_attr['config'],
        config_attr['type'],
        config_attr['default'],
        config_attr['no_spl_support'],
        jobs)

    # All the defconfig files to be processed
    defconfigs = []
    for (dirpath, dirnames, filenames) in os.walk('configs'):
        dirpath = dirpath[len('configs') + 1:]
        for filename in fnmatch.filter(filenames, '*_defconfig'):
            if fnmatch.fnmatch(filename, '.*'):
                continue
            defconfigs.append(os.path.join(dirpath, filename))

    slots = Slots(config_attr, jobs)

    # Main loop to process defconfig files:
    #  Add a new subprocess into a vacant slot.
    #  Sleep if there is no available slot.
    for defconfig in defconfigs:
        while not slots.add(defconfig):
            while not slots.available():
                # No available slot: sleep for a while
                time.sleep(SLEEP_TIME)

    # wait until all the subprocesses finish
    while not slots.empty():
        time.sleep(SLEEP_TIME)

    cleanup_headers(config_attr['config'])

def main():
    try:
        cpu_count = multiprocessing.cpu_count() + 4
    except NotImplementedError:
        cpu_count = 1

    parser = optparse.OptionParser()
    # Add options here
    parser.add_option('-j', '--jobs', type='int', default=cpu_count,
                      help='the number of jobs to run simultaneously')
    parser.usage += ' config type default no_spl_support'
    (options, args) = parser.parse_args()

    args_key = ('config', 'type', 'default', 'no_spl_support')
    config_attr = {}

    if len(args) >= len(args_key):
        saved_attr = ''
        for i, key in enumerate(args_key):
            config_attr[key] = args[i]
            saved_attr = saved_attr + ' %s' % args[i]
        with open('.moveconfig', 'w') as f:
            f.write(saved_attr)
    elif os.path.exists('.moveconfig'):
        f = open('.moveconfig')
        try:
            saved_attr = f.readline().split()
            for i, key in enumerate(args_key):
                config_attr[key] = saved_attr[i]
        except:
            sys.exit('%s: broken format' % '.moveconfig')
    else:
        parser.print_usage()
        sys.exit(1)

    if not config_attr['config'].startswith('CONFIG_'):
        config_attr['config'] = 'CONFIG_' + config_attr['config']

    move_config(config_attr, options.jobs)

if __name__ == '__main__':
    main()
