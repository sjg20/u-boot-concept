#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""Script to parse rename.syms files and generate API headers"""

import argparse
import filecmp
import os
import re
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from itertools import groupby
from typing import List

# Add the tools directory to the path for u_boot_pylib
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))

# pylint: disable=wrong-import-position,import-error
from u_boot_pylib import tools
from u_boot_pylib import test_util

# API header template parts
API_HEADER = '''#ifndef __ULIB_API_H
#define __ULIB_API_H

#include <stdarg.h>
#include <stddef.h>

/* Auto-generated header with renamed U-Boot library functions */

'''

API_FOOTER = '''#endif /* __ULIB_API_H */
'''

def rename_function(src, old_name, new_name):
    """Rename a function in C source code

    Args:
        src (str): The source code containing the function
        old_name (str): Current function name to rename
        new_name (str): New function name

    Returns:
        str: Source code with the function renamed
    """
    # Pattern to match function declaration/definition
    # Matches: return_type func(parameters)
    pattern = r'\b' + re.escape(old_name) + r'\b(?=\s*\()'

    # Replace all occurrences of the function name (in function comment too)
    renamed_code = re.sub(pattern, new_name, src)
    return renamed_code


@dataclass
class Symbol:
    """Represents a symbol rename operation for library functions.

    Used to track how functions from a header file should be renamed
    to create a namespaced API (e.g., printf -> ub_printf).
    """
    hdr: str        # Header file containing the function
    orig: str       # Original function name
    new_name: str   # New function name after renaming


class RenameSymsParser:
    """Parser for rename.syms files.

    Format:
        file: header.h
         symbol1
         symbol2=renamed_symbol2
         symbol3

    Lines starting with 'file:' specify a header file.
    Lines indented with space or tab specify symbols from that header.
    Symbol lines can use '=' for explicit renaming, otherwise 'ub_' prefix is
    added.
    Comments start with '#' and must begin at start of line.
    Empty lines are allowed.
    Trailing spaces are stripped but no other whitespace is allowed except for
    symbol indentation.
    """
    def __init__(self, fname: str):
        """Initialize the parser with a rename.syms file path

        Args:
            fname (str): Path to the rename.syms file to parse
        """
        self.fname = fname
        self.syms: List[Symbol] = []

    def parse_line(self, line: str, hdr: str) -> Symbol:
        """Parse a line and return a Symbol or None

        Args:
            line (str): The line to parse (already stripped)
            hdr (str): Current header file name

        Returns:
            Symbol or None: Symbol if line contains a symbol definition,
                None otherwise
        """
        if '=' in line:
            # Explicit mapping: orig=new
            orig, new = line.split('=', 1)
            orig = orig.strip()
            new = new.strip()
        else:
            # Default mapping: add 'ub_' prefix
            orig = line
            new = f'ub_{orig}'

        return Symbol(hdr=hdr, orig=orig, new_name=new)

    def parse(self) -> List[Symbol]:
        """Parse the rename.syms file and return list of symbols

        Returns:
            List[Symbol]: List of symbol rename operations
        """
        hdr = None
        content = tools.read_file(self.fname, binary=False)
        for line_num, line in enumerate(content.splitlines(), 1):
            line = line.rstrip()

            # Skip empty lines and comments
            if not line or line.startswith('#'):
                continue

            # Check for file directive
            if line.startswith('file:'):
                hdr = line.split(':', 1)[1].strip()
                continue

            # Check for symbol (indented line with space or tab)
            if line[0] not in [' ', '\t']:
                # Non-indented, non-file lines are invalid
                raise ValueError(f'Line {line_num}: Invalid format - '
                                 f"symbols must be indented: '{line}'")

            if hdr is None:
                raise ValueError(f"Line {line_num}: Symbol '{line.strip()}' "
                                 f'found without a header file directive')

            # Process valid symbol
            symbol = self.parse_line(line.strip(), hdr)
            if symbol:
                self.syms.append(symbol)
        return self.syms

    def dump(self):
        """Print the parsed symbols in a formatted way"""
        print(f'Parsed {len(self.syms)} symbols from '
              f'{self.fname}:')
        print()
        hdr = None
        for sym in self.syms:
            if sym.hdr != hdr:
                hdr = sym.hdr
                print(f'Header: {hdr}')
            print(f'  {sym.orig} -> {sym.new_name}')
        print(f'\nTotal: {len(self.syms)} symbols')


class DeclExtractor:
    """Extracts function declarations from header files with comments

    Expects functions to have an optional preceding comment block (either /**/
    style or // single-line) followed immediately by the function declaration.
    The declaration may span multiple lines until a semicolon or opening brace.

    Properties:
        lines (str): List of lines from the header file, set by extract()
    """

    def __init__(self, fname: str):
        """Initialize with header file path

        Args:
            fname (str): Path to the header file
        """
        self.fname = fname
        self.lines = []

    def find_function(self, func: str):
        """Find the line index of a function declaration

        Args:
            func (str): Name of the function to find

        Returns:
            int or None: Line index of function declaration, or None if
                not found
        """
        pattern = r'\b' + re.escape(func) + r'\s*\('

        for i, full_line in enumerate(self.lines):
            line = full_line.strip()
            # Skip comment lines and find actual function declarations
            if (not line.startswith('*') and not line.startswith('//') and
                    re.search(pattern, full_line)):
                return i

        return None

    def find_preceding_comment(self, func_idx: int):
        """Find comment block preceding a function declaration

        Args:
            func_idx (int): Line index of the function declaration

        Returns:
            int or None: Start line index of comment block, or None if not found
        """
        # Search backwards from the line before the function declaration
        for i in range(func_idx - 1, -1, -1):
            line = self.lines[i].strip()
            if not line:
                continue  # Skip empty lines
            if line.startswith('*/'):
                # Find the start of this comment block
                for j in range(i, -1, -1):
                    if '/**' in self.lines[j]:
                        return j
                break
            if line.startswith('//'):
                # Found single-line comment, include it if it's the first
                # non-empty line before function
                return i
            if not line.startswith('*'):
                # Hit non-comment content, no preceding comment
                break
        return None

    def extract_lines(self, start_idx: int, func_idx: int):
        """Extract comment and function declaration lines

        Args:
            start_idx (int): Starting line index (comment or function)
            func_idx (int): Function declaration line index

        Returns:
            str: Lines containing the complete declaration joined with newlines
        """
        lines = []

        # Add comment lines if found
        if start_idx < func_idx:
            lines.extend(self.lines[start_idx:func_idx])

        # Add function declaration lines
        for line in self.lines[func_idx:func_idx + 10]:
            lines.append(line)
            if ';' in line or '{' in line:
                break

        return '\n'.join(lines)

    def extract(self, func: str):
        """Find a function declaration in a header file, including its comment

        Args:
            func (str): Name of the function to find

        Returns:
            str or None: The function declaration with its comment, or None
                if not found
        """
        self.lines = tools.read_file(self.fname, binary=False).split('\n')

        func_idx = self.find_function(func)
        if func_idx is None:
            return None

        comment_idx = self.find_preceding_comment(func_idx)
        start_idx = comment_idx if comment_idx is not None else func_idx

        return self.extract_lines(start_idx, func_idx)

    @staticmethod
    def extract_decl(fname, func):
        """Find a function declaration in a header file, including its comment

        Args:
            fname (str): Path to the header file
            func (str): Name of the function to find

        Returns:
            str or None: The function declaration with its comment, or None
                if not found
        """
        extractor = DeclExtractor(fname)
        return extractor.extract(func)


class SymbolRedefiner:
    """Applies symbol redefinitions to object files using objcopy

    Processes object files to rename symbols using objcopy --redefine-sym.
    Always copies modified files to an output directory.

    Properties:
        redefine_args (List[str]): objcopy arguments for symbol redefinition
        symbol_names (set): Set of original symbol names to look for
    """

    def __init__(self, syms: List[Symbol], outdir: str, max_workers,
                 verbose=False):
        """Initialize with symbols and output settings

        Args:
            syms (List[Symbol]): List of symbols to redefine
            outdir (str): Directory to write modified object files
            max_workers (int): Number of parallel workers
            verbose (bool): Whether to show verbose output
        """
        self.syms = syms
        self.outdir = outdir
        self.verbose = verbose
        self.max_workers = max_workers
        self.redefine_args = []
        self.symbol_names = set()

        # Build objcopy command arguments and symbol set
        for sym in syms:
            self.redefine_args.extend(['--redefine-sym',
                                       f'{sym.orig}={sym.new_name}'])
            self.symbol_names.add(sym.orig)

    def redefine_file(self, infile: str, outfile: str):
        """Apply symbol redefinitions to a single object file

        Args:
            infile (str): Input object file path
            outfile (str): Output object file path
        """
        cmd = ['objcopy'] + self.redefine_args + [infile, outfile]
        subprocess.run(cmd, check=True, capture_output=True, text=True)
        if self.verbose:
            print(f'Copied and modified {infile} -> {outfile}')

    def _process_single_file(self, path: str, outfile: str) -> bool:
        """Process a single file (for parallel execution)

        Args:
            path (str): Input file path
            outfile (str): Output file path

        Returns:
            bool: True if file was modified, False otherwise
        """
        # Always run objcopy to apply redefinitions
        self.redefine_file(path, outfile)

        # Check if the file was actually modified
        return not filecmp.cmp(path, outfile, shallow=False)

    def process(self, work_items: List[tuple[str, str]]) -> \
            tuple[List[str], int]:
        """Process object files and apply symbol redefinitions

        Args:
            work_items (List[tuple[str, str]]): List of
                (input_path, output_path) tuples

        Returns:
            tuple[List[str], int]: List of output object file paths and
                count of modified files
        """
        # Process files in parallel
        outfiles = []
        modified = 0

        with ThreadPoolExecutor(max_workers=self.max_workers) as executor:
            # Submit all jobs
            future_to_item = {
                executor.submit(self._process_single_file, path, outfile):
                (path, outfile)
                for path, outfile in work_items
            }

            # Collect results
            for future in as_completed(future_to_item):
                path, outfile = future_to_item[future]
                was_modified = future.result()
                if was_modified:
                    modified += 1
                outfiles.append(outfile)

        # Sort outfiles to maintain consistent order
        outfiles.sort()
        return outfiles, modified

    @staticmethod
    def apply_renames(obj_files, syms, outdir: str, max_workers, verbose=False):
        """Apply symbol redefinitions to object files using objcopy

        Args:
            obj_files (List[str]): List of object file paths
            syms (List[Symbol]): List of symbols
            outdir (str): Directory to write modified object files
            max_workers (int): Number of parallel workers
            verbose (bool): Whether to show verbose output

        Returns:
            tuple[List[str], int]: List of output object file paths and
                count of modified files
        """
        if not syms:
            return obj_files, 0

        redefiner = SymbolRedefiner(syms, outdir, max_workers, verbose)

        # Setup: create output directory and prepare work items
        os.makedirs(outdir, exist_ok=True)

        # Prepare work items - just input and output paths
        work_items = []
        for path in obj_files:
            uniq = os.path.relpath(path).replace('/', '_')
            outfile = os.path.join(outdir, uniq)
            work_items.append((path, outfile))

        return redefiner.process(work_items)


class ApiGenerator:
    """Generates API headers with renamed function declarations

    Processes symbols and creates a unified header file with renamed function
    declarations extracted from original header files.
    """

    def __init__(self, syms: List[Symbol], include_dir: str, verbose=False):
        """Initialize with symbols and include directory

        Args:
            syms (List[Symbol]): List of symbols
            include_dir (str): Directory to search for header files
            verbose (bool): Whether to print status messages
        """
        self.syms = syms
        self.include_dir = include_dir
        self.verbose = verbose
        self.missing_decls = []
        self.missing_hdrs = []

    def process_header(self, hdr: str, header_syms: List[Symbol]):
        """Process a single header file and its symbols

        Args:
            hdr (str): Header file name
            header_syms (List[Symbol]): Symbols from this header

        Returns:
            List[str]: Lines for this header section
        """
        lines = [f'/* Functions from {hdr} */']

        path = os.path.join(self.include_dir, hdr)
        if not os.path.exists(path):
            self.missing_hdrs.append(hdr)
        else:
            # Extract and rename declarations from the actual header
            for sym in header_syms:
                orig = DeclExtractor.extract_decl(
                    path, sym.orig)
                if orig:
                    # Rename the function in the declaration
                    renamed_decl = rename_function(
                        orig, sym.orig, sym.new_name)
                    lines.append(renamed_decl)
                else:
                    self.missing_decls.append((sym.orig, hdr))
                lines.append('')

        lines.append('')
        return lines

    def check_errors(self):
        """Check for missing headers or declarations and build error message

        Returns:
            str: Error messages, or '' if None
        """
        msgs = []
        if self.missing_hdrs:
            msgs.append('')
            msgs.append('Missing header files:')
            for header in self.missing_hdrs:
                msgs.append(f'  - {header}')

        if self.missing_decls:
            msgs.append('')
            msgs.append('Missing function declarations:')
            for func_name, hdr in self.missing_decls:
                msgs.append(f'  - {func_name} in {hdr}')

        return '\n'.join(msgs)

    def generate(self, outfile: str):
        """Generate the API header file

        Args:
            outfile (str): Path where to write the new header file

        Returns:
            int: 0 on success, 1 on error
        """
        # Process each header file
        out = []
        sorted_syms = sorted(self.syms, key=lambda s: s.hdr)
        by_header = {hdr: list(syms)
                     for hdr, syms in groupby(sorted_syms, key=lambda s: s.hdr)}
        for hdr, syms in by_header.items():
            out.extend(self.process_header(hdr, syms))

        # Check for errors and abort if any declarations are missing
        error_msg = self.check_errors()
        if error_msg:
            print(error_msg, file=sys.stderr)
            return 1

        # Write the header file
        content = API_HEADER + '\n'.join(out) + API_FOOTER
        tools.write_file(outfile, content, binary=False)
        if self.verbose:
            print(f'Generated API header: {outfile}')

        return 0

    @staticmethod
    def generate_hdr(syms, include_dir, outfile, verbose=False):
        """Generate a new header file with renamed function declarations

        Args:
            syms (List[Symbol]): List of symbols
            include_dir (str): Directory to search for header files
            outfile (str): Path where to write the new header file
            verbose (bool): Whether to print status messages

        Returns:
            int: 0 on success, 1 on error
        """
        if not syms:
            print('Warning: No symbols found', file=sys.stderr)
            return 0

        generator = ApiGenerator(syms, include_dir, verbose)
        return generator.generate(outfile)


def run_tests(processes, test_name):  # pragma: no cover
    """Run all the tests we have for build_api

    Args:
        processes (int): Number of processes to use to run tests
        test_name (str): Name of specific test to run, or None to run all tests

    Returns:
        int: 0 if successful, 1 if not
    """
    # pylint: disable=import-outside-toplevel,import-error
    # Import our test module
    test_dir = os.path.join(os.path.dirname(__file__), '../test/scripts')
    sys.path.insert(0, test_dir)

    import test_build_api

    sys.argv = [sys.argv[0]]

    result = test_util.run_test_suites(
        toolname='build_api', debug=True, verbosity=2, no_capture=False,
        test_preserve_dirs=False, processes=processes, test_name=test_name,
        toolpath=[],
        class_and_module_list=[test_build_api.TestBuildApi])

    return 0 if result.wasSuccessful() else 1


def run_test_coverage():  # pragma: no cover
    """Run the tests and check that we get 100% coverage"""
    sys.argv = [sys.argv[0]]
    test_util.run_test_coverage('scripts/build_api.py', None,
            ['tools/u_boot_pylib/*', '*/test*'], '.')


def parse_args(argv):
    """Parse and validate command line arguments

    Args:
        argv (List[str]): Arguments to parse

    Returns:
        tuple: (args, error_code) where args is argparse.Namespace or None,
               and error_code is 0 for success or 1 for error
    """
    parser = argparse.ArgumentParser(
        description='Parse rename.syms file and show symbols')
    parser.add_argument('rename_syms', nargs='?',
                        help='Path to rename.syms file')
    parser.add_argument('-d', '--dump', action='store_true',
                        help='Dump parsed symbols')
    parser.add_argument('-r', '--redefine', nargs='*', metavar='OBJ_FILE',
                        help='Apply symbol redefinitions to object files')
    parser.add_argument('-a', '--api', metavar='HEADER_FILE',
                        help='Generate API header with renamed functions')
    parser.add_argument('-i', '--include-dir', metavar='DIR',
                        help='Include directory containing header files')
    parser.add_argument('-o', '--output-dir', metavar='DIR',
                        help='Output directory for modified object files')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Show verbose output')
    parser.add_argument('-j', '--jobs', type=int, metavar='N',
                        help='Number of parallel jobs for symbol processing')
    parser.add_argument('-P', '--processes', type=int,
                        help='set number of processes to use for running tests')
    parser.add_argument('-t', '--test', action='store_true', dest='test',
                        default=False, help='run tests')
    parser.add_argument('-T', '--test-coverage', action='store_true',
                        default=False,
                        help='run tests and check for 100%% coverage')
    args = parser.parse_args(argv)

    # Check if running tests - if so, rename_syms is optional
    running_tests = args.test or args.test_coverage

    if not running_tests and not args.rename_syms:  # pragma: no cover
        print('Error: rename_syms is required unless running tests',
              # pragma: no cover
              file=sys.stderr)  # pragma: no cover
        return None, 1  # pragma: no cover

    # Validate argument combinations
    if args.redefine is not None and not args.redefine:
        # args.redefine is [] when --redefine used with no object files
        print('Error: --redefine requires at least one object file',
              file=sys.stderr)
        return None, 1

    if args.redefine is not None and not args.output_dir:
        print('Error: --output-dir is required with --redefine',
              file=sys.stderr)
        return None, 1

    if args.api and not args.include_dir:
        print('Error: --include-dir is required with --api',
              file=sys.stderr)
        return None, 1

    return args, 0


def main(argv=None):
    """Main entry point for the script

    Args:
        argv (List[str], optional): Arguments to parse. Uses sys.argv[1:]
            if None.

    Returns:
        int: Exit code (0 for success, 1 for error)
    """
    if argv is None:
        argv = sys.argv[1:]
    args, error_code = parse_args(argv)
    if error_code:
        return error_code

    # Handle test options
    if args.test:  # pragma: no cover
        test_name = args.rename_syms # pragma: no cover
        return run_tests(args.processes, test_name)  # pragma: no cover

    if args.test_coverage:  # pragma: no cover
        run_test_coverage()  # pragma: no cover
        return 0  # pragma: no cover

    symbols_parser = RenameSymsParser(args.rename_syms)
    syms = symbols_parser.parse()

    if args.dump:
        symbols_parser.dump()

    if args.redefine is not None:
        # Determine number of jobs
        jobs = args.jobs if args.jobs else min(os.cpu_count() or 4, 8)
        start_time = time.time()
        outfiles, modified = SymbolRedefiner.apply_renames(
            args.redefine, syms, args.output_dir, jobs, args.verbose)
        # Print the list of output files for the build system to use
        if args.output_dir:
            print('\n'.join(outfiles))
        elapsed = time.time() - start_time
        if args.verbose:
            print(f'Processed {len(args.redefine)} files ({modified} modified) '
                  f'in {elapsed:.3f} seconds ({jobs} threads)', file=sys.stderr)

    if args.api:
        result = ApiGenerator.generate_hdr(syms, args.include_dir, args.api,
                                           args.verbose)
        if result:
            return result

    return 0


if __name__ == '__main__':  # pragma: no cover
    sys.exit(main())
