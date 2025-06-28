#!/usr/bin/env python3
#
# check_list_alignment.py: Auto-discover and verify the uniform
# spacing of all U-Boot linker list symbols
#
# Analyze the symbol table of a U-Boot ELF file to ensure that
# all entries in all linker-generated lists are separated by a consistent
# number of bytes. Detect problems caused by linker-inserted
# alignment padding.
#
# By default, produce no output if no problems are found
# Use the -v flag to force output even on success
#
# Exit Codes:
#   0: Success. No alignment problems were found
#   1: Usage Error. The script was not called with the correct arguments
#   2: Execution Error. Failed to run `nm` or the ELF file was not found
#   3: Problem Found. An inconsistent gap was detected in at least one list
#

import sys
import subprocess
import re
import argparse
from statistics import mode, StatisticsError
from collections import defaultdict

def eprint(*args, **kwargs):
    """Print to stderr"""
    print(*args, file=sys.stderr, **kwargs)

def check_single_list(name, symbols, max_name_len):
    """Check alignment for a single list and return its findings

    Args:
        name (str): The cleaned-up name of the list for display
        symbols (list): A list of (address, name) tuples, sorted by address
        max_name_len (int): The max length of list names for column formatting

    Returns:
        tuple: (problem_count, list_of_output_lines)
    """
    lines = []
    if len(symbols) < 2:
        return 0, []

    gaps = []
    for i in range(len(symbols) - 1):
        addr1, name1 = symbols[i]
        addr2, name2 = symbols[i+1]
        gap = addr2 - addr1
        gaps.append({'gap': gap, 'prev_sym': name1, 'next_sym': name2})

    try:
        expected_gap = mode(g['gap'] for g in gaps)
        name_col = f"{name}"
        symbols_col = f"{len(symbols)}"
        size_col = f"0x{expected_gap:x}"
        lines.append(
            f"{name_col:<{max_name_len + 2}}  {symbols_col:>12}  "
            f"{size_col:>17}")

    except StatisticsError:
        lines.append(f"\n!!! PROBLEM DETECTED IN LIST '{name}' !!!")
        lines.append(
            "  Error: Could not determine a common element size. "
            "All gaps are unique")
        for g in gaps:
            lines.append(
                f"  - Gap of 0x{g['gap']:x} bytes between {g['prev_sym']} "
                f"and {g['next_sym']}")
        return len(gaps), lines

    problem_count = 0
    for g in gaps:
        if g['gap'] != expected_gap:
            problem_count += 1
            lines.append(
                f"  - Bad gap (0x{g['gap']:x}) before symbol: {g['next_sym']}")

    return problem_count, lines


def discover_and_check_all_lists(elf_path, verbose):
    """Run `nm`, discover all linker lists, and check each one

    Print output only if problems are found or verbose mode is enabled
    """
    cmd = ['nm', '-n', elf_path]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, check=True)
    except FileNotFoundError:
        eprint(
            "Error: The 'nm' command was not found. "
            "Please ensure binutils is installed")
        return 2
    except subprocess.CalledProcessError as e:
        eprint(
            f"Error: Failed to execute 'nm' on '{elf_path}'.\n"
            f"  Return Code: {e.returncode}\n  Stderr:\n{e.stderr}")
        return 2

    # A pattern to find the base name of a U-Boot list symbol
    # It captures the part before '(_info)?_2_'
    list_name_pattern = re.compile(
        r'^(?P<base_name>_u_boot_list_\d+_\w+)(?:_info)?_2_')
    lists = defaultdict(list)
    for line in proc.stdout.splitlines():
        if ' D _u_boot_list_' not in line:
            continue
        try:
            parts = line.strip().split()
            address, name = int(parts[0], 16), parts[-1]

            match = list_name_pattern.match(name)
            if match:
                base_name = match.group('base_name')
                lists[base_name].append((address, name))
        except (ValueError, IndexError):
            eprint(f"Warning: Could not parse line: {line}")

    if not lists:
        if verbose:
            eprint("Success: No U-Boot linker lists found to check")
        return 0

    names = {}
    prefix_to_strip = '_u_boot_list_2_'
    for list_name in lists.keys():
        name = list_name[len(prefix_to_strip):] if list_name.startswith(
            prefix_to_strip) else list_name
        names[list_name] = name

    max_name_len = max(len(name) for name in names.values()) if names else 0

    # --- Data Collection Phase ---
    total_problems = 0
    total_symbols = 0
    all_lines = []
    for list_name in sorted(lists.keys()):
        symbols = lists[list_name]
        total_symbols += len(symbols)
        name = names[list_name]
        problem_count, lines = check_single_list(name, symbols, max_name_len)
        total_problems += problem_count
        all_lines.extend(lines)

    # --- Output Phase ---
    if total_problems > 0 or verbose:
        header = (f"{'List Name':<{max_name_len + 2}}  {'# Symbols':>12}  "
                  f"{'Struct Size (hex)':>17}")
        eprint(header)
        eprint(f"{'-' * (max_name_len + 2)}  {'-' * 12}  {'-' * 17}")
        for line in all_lines:
            eprint(line)

        # Print footer
        eprint(f"{'-' * (max_name_len + 2)}  {'-' * 12}")
        name_col = f"{len(lists)} lists"
        symbols_col = f"{total_symbols}"
        eprint(f"{name_col:<{max_name_len + 2}}  {symbols_col:>12}")

    if total_problems > 0:
        eprint(f"\nFAILURE: Found {total_problems} alignment problems")
        return 3

    if verbose:
        eprint(
            f"\nSUCCESS: All discovered linker lists have consistent alignment")

    return 0

def main():
    """Main entry point of the script"""
    epilog_text = """
Auto-discover all linker-generated lists in a U-Boot ELF file
(e.g., for drivers, commands, etc.) and verify their integrity. Check
that all elements in a given list are separated by a consistent number of
bytes.

Problems typically indicate that the linker has inserted alignment padding
between two elements in a list, which can break U-Boot's assumption that the
list is a simple, contiguous array of same-sized structs.
"""
    parser = argparse.ArgumentParser(
        description="Check alignment of all U-Boot linker lists in an ELF file.",
        epilog=epilog_text,
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument('elf_path', metavar='path_to_elf_file',
                        help='Path to the U-Boot ELF file to check')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Print detailed output even on success')

    args = parser.parse_args()

    exit_code = discover_and_check_all_lists(args.elf_path, args.verbose)
    sys.exit(exit_code)

if __name__ == "__main__":
    main()
