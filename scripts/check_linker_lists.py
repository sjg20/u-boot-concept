#!/usr/bin/env python3
#
# check_list_alignment.py: A script to auto-discover and verify the uniform
# spacing of all U-Boot linker list symbols.
#
# This script analyzes the symbol table of a U-Boot ELF file to ensure that
# all entries in all linker-generated lists are separated by a consistent
# number of bytes. It is designed to detect problems caused by linker-inserted
# alignment padding.
#
# By default, it produces no output if no problems are found.
# Use the -v flag to force output even on success.
#
# Exit Codes:
#   0: Success. No alignment problems were found.
#   1: Usage Error. The script was not called with the correct arguments.
#   2: Execution Error. Failed to run `nm` or the ELF file was not found.
#   3: Problem Found. An inconsistent gap was detected in at least one list.
#

import sys
import subprocess
import re
import argparse
from statistics import mode, StatisticsError
from collections import defaultdict

def check_single_list(display_name, symbols, max_name_len):
    """
    Checks alignment for a single list and returns its findings.

    Args:
        display_name (str): The cleaned-up name of the list for display.
        symbols (list): A list of (address, name) tuples, sorted by address.
        max_name_len (int): The max length of list names for column formatting.

    Returns:
        tuple: (problem_count, list_of_output_lines)
    """
    output_lines = []
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
        name_col = f"{display_name}"
        symbols_col = f"{len(symbols)}"
        size_col = f"0x{expected_gap:x}"
        output_lines.append(f"{name_col:<{max_name_len + 2}}  {symbols_col:>12}  {size_col:>17}")

    except StatisticsError:
        output_lines.append(f"\n!!! PROBLEM DETECTED IN LIST '{display_name}' !!!")
        output_lines.append("  Error: Could not determine a common element size. All gaps are unique.")
        for g in gaps:
            output_lines.append(f"  - Gap of 0x{g['gap']:x} bytes between {g['prev_sym']} and {g['next_sym']}")
        return len(gaps), output_lines

    problem_count = 0
    for g in gaps:
        if g['gap'] != expected_gap:
            problem_count += 1
            output_lines.append(f"  - Bad gap (0x{g['gap']:x}) before symbol: {g['next_sym']}")

    return problem_count, output_lines


def discover_and_check_all_lists(elf_path, verbose):
    """
    Runs `nm`, discovers all linker lists, checks each one, and prints output
    only if problems are found or verbose mode is enabled.
    """
    cmd = ['nm', '-n', elf_path]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, check=True)
    except FileNotFoundError:
        print("Error: The 'nm' command was not found. Please ensure binutils is installed.", file=sys.stderr)
        return 2
    except subprocess.CalledProcessError as e:
        print(f"Error: Failed to execute 'nm' on '{elf_path}'.\n  Return Code: {e.returncode}\n  Stderr:\n{e.stderr}", file=sys.stderr)
        return 2

    discovered_lists = defaultdict(list)
    for line in proc.stdout.splitlines():
        if ' D _u_boot_list_' not in line:
            continue
        try:
            parts = line.strip().split()
            address, name = int(parts[0], 16), parts[-1]
            base_name = None
            if '_info_2_' in name:
                base_name = name.rsplit('_info_2_', 1)[0]
            elif '_2_' in name:
                base_name = name.rsplit('_2_', 1)[0]
            if base_name:
                discovered_lists[base_name].append((address, name))
        except (ValueError, IndexError):
            print(f"Warning: Could not parse line: {line}", file=sys.stderr)

    if not discovered_lists:
        if verbose:
            print("Success: No U-Boot linker lists found to check.", file=sys.stderr)
        return 0

    display_names = {}
    prefix_to_strip = '_u_boot_list_2_'
    for list_name in discovered_lists.keys():
        display_name = list_name[len(prefix_to_strip):] if list_name.startswith(prefix_to_strip) else list_name
        display_names[list_name] = display_name

    max_name_len = max(len(name) for name in display_names.values()) if display_names else 0

    # --- Data Collection Phase ---
    total_problems = 0
    total_symbols = 0
    all_output_lines = []
    for list_name in sorted(discovered_lists.keys()):
        symbols = discovered_lists[list_name]
        total_symbols += len(symbols)
        display_name = display_names[list_name]
        problem_count, output_lines = check_single_list(display_name, symbols, max_name_len)
        total_problems += problem_count
        all_output_lines.extend(output_lines)

    # --- Output Phase ---
    if total_problems > 0 or verbose:
        print(f"{'List Name':<{max_name_len + 2}}  {'# Symbols':>12}  {'Struct Size (hex)':>17}", file=sys.stderr)
        print(f"{'-' * (max_name_len + 2)}  {'-' * 12}  {'-' * 17}", file=sys.stderr)
        for line in all_output_lines:
            print(line, file=sys.stderr)

        # Print footer
        print(f"{'-' * (max_name_len + 2)}  {'-' * 12}", file=sys.stderr)
        footer_name_col = f"{len(discovered_lists)} lists"
        footer_symbols_col = f"{total_symbols}"
        print(f"{footer_name_col:<{max_name_len + 2}}  {footer_symbols_col:>12}", file=sys.stderr)

    if total_problems > 0:
        print(f"\nFAILURE: Found {total_problems} alignment problems.", file=sys.stderr)
        return 3

    if verbose:
        print(f"\nSUCCESS: All discovered linker lists have consistent alignment.", file=sys.stderr)

    return 0

def main():
    """ Main entry point of the script. """
    epilog_text = """
This script auto-discovers all linker-generated lists in a U-Boot ELF file
(e.g., for drivers, commands, etc.) and verifies their integrity. It checks
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
                        help='Path to the U-Boot ELF file to check.')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Print detailed output even on success.')

    args = parser.parse_args()

    exit_code = discover_and_check_all_lists(args.elf_path, args.verbose)
    sys.exit(exit_code)

if __name__ == "__main__":
    main()
