#!/usr/bin/env python3
#
# check_list_alignment.py: A script to auto-discover and verify the uniform
# spacing of all U-Boot linker list symbols.
#
# This script analyzes the symbol table of a U-Boot ELF file to ensure that
# all entries in all linker-generated lists are separated by a consistent
# number of bytes. It is designed to detect anomalies caused by linker-inserted
# alignment padding.
#
# It works by:
# 1. Executing `nm -n` once on the provided ELF file to get a list of all
#    symbols sorted by their virtual address.
# 2. Automatically discovering all unique linker list base names (e.g.,
#    '_u_boot_list_2_driver') from the symbol names.
# 3. For each discovered list:
#    a. It filters the symbols belonging to that list.
#    b. It calculates the address difference (gap) between consecutive symbols.
#    c. It determines the most common gap size (the mode), which is assumed
#       to be the correct size of the list's struct element.
#    d. It compares every gap to this mode. If any gap is different, it
#       reports an anomaly.
#
# The script will exit with a non-zero code if an anomaly is found in *any* list.
#
# Exit Codes:
#   0: Success. No alignment anomalies were found in any list.
#   1: Usage Error. The script was not called with the correct argument.
#   2: Execution Error. Failed to run `nm` or the ELF file was not found.
#   3: Anomaly Found. An inconsistent gap was detected in at least one list.
#

import sys
import subprocess
import re
from statistics import mode, StatisticsError
from collections import defaultdict

def check_single_list(display_name, symbols, max_name_len):
    """
    Checks alignment for a single, pre-filtered list of symbols.

    Args:
        display_name (str): The cleaned-up name of the list for display.
        symbols (list): A list of (address, name) tuples, sorted by address.
        max_name_len (int): The max length of list names for column formatting.

    Returns:
        int: The number of anomalies found in this list.
    """
    if len(symbols) < 2:
        return 0

    gaps = []
    for i in range(len(symbols) - 1):
        addr1, name1 = symbols[i]
        addr2, name2 = symbols[i+1]
        gap = addr2 - addr1
        gaps.append({'gap': gap, 'prev_sym': name1, 'next_sym': name2})

    try:
        expected_gap = mode(g['gap'] for g in gaps)
        # Print the concise, column-formatted check line
        name_col = f"{display_name}"
        symbols_col = f"{len(symbols)}"
        size_col = f"0x{expected_gap:x}"
        print(f"{name_col:<{max_name_len + 2}}  {symbols_col:>12}  {size_col:>17}", file=sys.stderr)

    except StatisticsError:
        print(f"\n!!! ANOMALY DETECTED IN LIST '{display_name}' !!!", file=sys.stderr)
        print("  Error: Could not determine a common element size. All gaps are unique.", file=sys.stderr)
        for g in gaps:
            print(f"  - Gap of 0x{g['gap']:x} bytes between {g['prev_sym']} and {g['next_sym']}", file=sys.stderr)
        return len(gaps)

    anomaly_count = 0
    first_anomaly_printed = False
    for g in gaps:
        if g['gap'] != expected_gap:
            if not first_anomaly_printed:
                # This check ensures the "ANOMALY DETECTED" header is printed only once per list
                print(f"!!! ANOMALY DETECTED IN LIST '{display_name}' !!!", file=sys.stderr)
                first_anomaly_printed = True
            anomaly_count += 1
            print(f"  - Inconsistent gap (0x{g['gap']:x}) before symbol: {g['next_sym']}", file=sys.stderr)

    return anomaly_count


def discover_and_check_all_lists(elf_path):
    """
    Runs `nm`, discovers all linker lists, and checks each one for alignment.
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
        print("Success: No U-Boot linker lists found to check.", file=sys.stderr)
        return 0

    display_names = {}
    prefix_to_strip = '_u_boot_list_2_'
    for list_name in discovered_lists.keys():
        display_name = list_name[len(prefix_to_strip):] if list_name.startswith(prefix_to_strip) else list_name
        display_names[list_name] = display_name

    max_name_len = max(len(name) for name in display_names.values()) if display_names else 0

    print(f"{'List Name':<{max_name_len + 2}}  {'# Symbols':>12}  {'Struct Size (hex)':>17}", file=sys.stderr)
    print(f"{'-' * (max_name_len + 2)}  {'-' * 12}  {'-' * 17}", file=sys.stderr)

    total_anomalies = 0
    for list_name in sorted(discovered_lists.keys()):
        symbols = discovered_lists[list_name]
        display_name = display_names[list_name]
        total_anomalies += check_single_list(display_name, symbols, max_name_len)

    if total_anomalies > 0:
        print(f"\nFAILURE: Found {total_anomalies} alignment anomalies.", file=sys.stderr)
        return 3

    print(f"\nSUCCESS: All discovered linker lists have consistent alignment.", file=sys.stderr)
    return 0

def main():
    """ Main entry point of the script. """
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <path_to_u-boot_elf_file>\nExample: {sys.argv[0]} u-boot", file=sys.stderr)
        sys.exit(1)

    elf_file = sys.argv[1]
    exit_code = discover_and_check_all_lists(elf_file)
    sys.exit(exit_code)

if __name__ == "__main__":
    main()
