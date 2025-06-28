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

def check_single_list(list_name, symbols):
    """
    Checks alignment for a single, pre-filtered list of symbols.

    Args:
        list_name (str): The base name of the list being checked.
        symbols (list): A list of (address, name) tuples, sorted by address.

    Returns:
        bool: True if an anomaly is found, False otherwise.
    """
    if len(symbols) < 2:
        # Not enough symbols to find a gap, so it's trivially correct.
        return False

    # Create a cleaner display name for the output by stripping the common prefix.
    display_name = list_name
    prefix_to_strip = '_u_boot_list_2_'
    if list_name.startswith(prefix_to_strip):
        display_name = list_name[len(prefix_to_strip):]

    print(f"\nChecking list '{display_name}' ({len(symbols)} symbols)...", file=sys.stderr)

    # Calculate the size difference between each consecutive symbol
    gaps = []
    for i in range(len(symbols) - 1):
        addr1, name1 = symbols[i]
        addr2, name2 = symbols[i+1]
        gap = addr2 - addr1
        gaps.append({'gap': gap, 'prev_sym': name1, 'next_sym': name2})

    # Determine the most common gap size (the mode).
    try:
        expected_gap = mode(g['gap'] for g in gaps)
        print(f"Determined expected element size (mode) = {expected_gap} bytes (0x{expected_gap:x})", file=sys.stderr)
    except StatisticsError:
        print(f"!!! ANOMALY DETECTED IN LIST '{display_name}' !!!", file=sys.stderr)
        print("  Error: Could not determine a common element size. All gaps are unique.", file=sys.stderr)
        for g in gaps:
            print(f"  - Gap of {g['gap']:>4} (0x{g['gap']:x}) bytes between {g['prev_sym']} and {g['next_sym']}", file=sys.stderr)
        return True # Anomaly found

    # Check for any gaps that don't match the expected size
    anomaly_found = False
    for g in gaps:
        if g['gap'] != expected_gap:
            if not anomaly_found: # Print header only once
                print(f"\n!!! ANOMALY DETECTED IN LIST '{display_name}' !!!", file=sys.stderr)
            anomaly_found = True
            print(f"  - Inconsistent gap found between symbols:", file=sys.stderr)
            print(f"    -> {g['prev_sym']}", file=sys.stderr)
            print(f"    -> {g['next_sym']}", file=sys.stderr)
            print(f"    Expected gap: {expected_gap:>4} (0x{expected_gap:x}) bytes", file=sys.stderr)
            print(f"    Actual gap:   {g['gap']:>4} (0x{g['gap']:x}) bytes", file=sys.stderr)
            padding = abs(g['gap'] - expected_gap)
            print(f"    This indicates {padding} bytes of unexpected linker padding.", file=sys.stderr)

    return anomaly_found


def discover_and_check_all_lists(elf_path):
    """
    Runs `nm`, discovers all linker lists, and checks each one for alignment.

    Args:
        elf_path (str): The path to the U-Boot ELF executable.

    Returns:
        int: An exit code (0 for success, 2 for error, 3 for anomaly).
    """
    print(f"Auto-discovering and checking all linker lists in: {elf_path}", file=sys.stderr)

    # We use `nm -n` to sort symbols numerically by address.
    cmd = ['nm', '-n', elf_path]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, check=True)
    except FileNotFoundError:
        print("Error: The 'nm' command was not found. Please ensure binutils is installed.", file=sys.stderr)
        return 2
    except subprocess.CalledProcessError as e:
        print(f"Error: Failed to execute 'nm' on '{elf_path}'.", file=sys.stderr)
        print(f"  Return Code: {e.returncode}\n  Stderr:\n{e.stderr}", file=sys.stderr)
        return 2

    # Discover all linker lists and group their symbols
    # The pattern is <list_base_name>[_info]_2_<entry_name>
    discovered_lists = defaultdict(list)

    for line in proc.stdout.splitlines():
        # Look for symbols in the data section that start with the list prefix
        if ' D _u_boot_list_' not in line:
            continue

        try:
            parts = line.strip().split()
            address = int(parts[0], 16)
            name = parts[-1]
            base_name = None

            # Split the name to find the base list name. This is more robust than a single regex.
            if '_info_2_' in name:
                base_name = name.rsplit('_info_2_', 1)[0]
            elif '_2_' in name:
                base_name = name.rsplit('_2_', 1)[0]

            if base_name:
                discovered_lists[base_name].append((address, name))

        except (ValueError, IndexError):
            print(f"Warning: Could not parse line: {line}", file=sys.stderr)
            continue

    if not discovered_lists:
        print("Success: No U-Boot linker lists found to check.", file=sys.stderr)
        return 0

    print(f"\nDiscovered {len(discovered_lists)} unique linker lists. Now checking each...", file=sys.stderr)

    overall_anomaly_found = False
    # Check each discovered list, sorted by name for deterministic output
    for list_name in sorted(discovered_lists.keys()):
        symbols = discovered_lists[list_name]
        if check_single_list(list_name, symbols):
            overall_anomaly_found = True

    if overall_anomaly_found:
        print(f"\nFAILURE: Exiting with status 3 due to alignment anomalies found.", file=sys.stderr)
        return 3

    print(f"\nSUCCESS: All {len(discovered_lists)} discovered linker lists have consistent alignment.", file=sys.stderr)
    return 0

def main():
    """ Main entry point of the script. """
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <path_to_u-boot_elf_file>", file=sys.stderr)
        print(f"Example: {sys.argv[0]} u-boot", file=sys.stderr)
        sys.exit(1)

    elf_file = sys.argv[1]
    exit_code = discover_and_check_all_lists(elf_file)
    sys.exit(exit_code)

if __name__ == "__main__":
    main()
