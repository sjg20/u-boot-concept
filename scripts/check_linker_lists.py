#!/usr/bin/env python3
#
# check_driver_alignment.py: A script to verify uniform spacing of U-Boot driver symbols.
#
# This script analyzes the symbol table of a U-Boot ELF file to ensure that
# all driver list entries are separated by a consistent number of bytes.
# It is designed to detect anomalies caused by linker-inserted alignment
# padding, which can disrupt the assumption of a contiguous array of
# same-sized structs.
#
# It works by:
# 1. Executing `nm -n` on the provided ELF file to get a list of symbols
#    sorted by their virtual address.
# 2. Filtering this list to find only the U-Boot driver entries.
# 3. Calculating the address difference (gap) between each consecutive driver symbol.
# 4. Determining the most common gap size (the mode), which is assumed to be
#    the correct `sizeof(struct udevice_driver)`.
# 5. Comparing every gap to this mode. If any gap is different, it signals
#    an alignment problem.
#
# Exit Codes:
#   0: Success. No alignment anomalies were found.
#   1: Usage Error. The script was not called with the correct arguments.
#   2: Execution Error. Failed to run `nm` or the ELF file was not found.
#   3: Anomaly Found. An inconsistent gap was detected between two symbols.
#

import sys
import subprocess
import re
from statistics import mode, StatisticsError

# This regex is designed to match the symbols for U-Boot driver entries.
# It specifically handles both '_driver_2_' and the less common '_driver_info_2_'
# naming conventions to ensure all relevant symbols are captured.
# Example matches:
#   _u_boot_list_2_driver_2_virtio_pci_modern
#   _u_boot_list_2_driver_info_2_x86_qfw_pio
DRIVER_SYMBOL_REGEX = re.compile(r'^[0-9a-fA-F]+\s+D\s+_u_boot_list_2_driver(_info)?_2_')

def check_alignment(elf_path):
    """
    Runs the main logic to check symbol alignment in the ELF file.

    Args:
        elf_path (str): The path to the U-Boot ELF executable.

    Returns:
        int: An exit code (0 for success, 2 for error, 3 for anomaly).
    """
    print(f"--- Analyzing driver alignment in: {elf_path}", file=sys.stderr)

    # We use `nm -n` to sort symbols numerically by address. This is critical
    # as it allows us to simply iterate through the list to find consecutive entries.
    cmd = ['nm', '-n', elf_path]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, check=True)
    except FileNotFoundError:
        print(f"Error: The 'nm' command was not found. Please ensure binutils is installed.", file=sys.stderr)
        return 2
    except subprocess.CalledProcessError as e:
        print(f"Error: Failed to execute 'nm' on '{elf_path}'.", file=sys.stderr)
        print(f"  Return Code: {e.returncode}", file=sys.stderr)
        print(f"  Stderr:\n{e.stderr}", file=sys.stderr)
        return 2

    # Parse the output from nm to find driver symbols
    driver_symbols = []
    for line in proc.stdout.splitlines():
        if DRIVER_SYMBOL_REGEX.search(line):
            try:
                parts = line.strip().split()
                # Address is the first part, name is the last part.
                address = int(parts[0], 16)
                name = parts[-1]
                driver_symbols.append((address, name))
            except (ValueError, IndexError):
                # This should not happen if the regex matches, but is a safeguard.
                print(f"Warning: Could not parse line: {line}", file=sys.stderr)
                continue

    if len(driver_symbols) < 2:
        print("Success: Not enough driver symbols found to perform a check.", file=sys.stderr)
        return 0

    print(f"--- Found {len(driver_symbols)} driver symbols.", file=sys.stderr)

    # Calculate the size difference between each consecutive symbol
    gaps = []
    for i in range(len(driver_symbols) - 1):
        addr1, name1 = driver_symbols[i]
        addr2, name2 = driver_symbols[i+1]
        gap = addr2 - addr1
        gaps.append({'gap': gap, 'prev_sym': name1, 'next_sym': name2})

    # Determine the most common gap size (the mode). This represents the
    # expected sizeof(struct udevice_driver) on this architecture.
    try:
        expected_gap = mode(g['gap'] for g in gaps)
        print(f"--- Determined expected struct size (mode) = {expected_gap} bytes (0x{expected_gap:x})", file=sys.stderr)
    except StatisticsError:
        # This occurs if all gaps are unique, which indicates a severe problem.
        print("Error: Could not determine a common driver struct size. All gaps are unique.", file=sys.stderr)
        for g in gaps:
            print(f"  - Gap of {g['gap']} bytes between {g['prev_sym']} and {g['next_sym']}", file=sys.stderr)
        return 3

    # Check for any gaps that don't match the expected size
    anomaly_found = False
    for g in gaps:
        if g['gap'] != expected_gap:
            anomaly_found = True
            print("\n!!! ANOMALY DETECTED !!!", file=sys.stderr)
            print(f"  Inconsistent gap found between symbols:", file=sys.stderr)
            print(f"    -> {g['prev_sym']}", file=sys.stderr)
            print(f"    -> {g['next_sym']}", file=sys.stderr)
            print(f"  Expected gap: {expected_gap} (0x{expected_gap:x}) bytes", file=sys.stderr)
            print(f"  Actual gap:   {g['gap']} (0x{g['gap']:x}) bytes", file=sys.stderr)
            padding = abs(g['gap'] - expected_gap)
            print(f"  This indicates {padding} bytes of unexpected linker padding.", file=sys.stderr)

    if anomaly_found:
        print("\n--- Exiting with status 3 due to alignment anomaly.", file=sys.stderr)
        return 3

    print("\n--- Success: All driver symbols have consistent alignment.", file=sys.stderr)
    return 0

def main():
    """ Main entry point of the script. """
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <path_to_u-boot_elf_file>", file=sys.stderr)
        sys.exit(1)

    elf_file = sys.argv[1]
    exit_code = check_alignment(elf_file)
    sys.exit(exit_code)

if __name__ == "__main__":
    main()

