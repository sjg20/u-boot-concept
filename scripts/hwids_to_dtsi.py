#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
"""
Convert HWIDS txt files to devicetree source (.dtsi) format

This script converts hardware ID files from board/efi/hwids/ into devicetree
source files. The output includes SMBIOS computer information as properties
and Hardware IDs as binary GUID arrays.
"""

import argparse
import glob
from io import StringIO
import os
import re
import sys
import traceback
import uuid
from enum import IntEnum


DTSI_HEADER = """// SPDX-License-Identifier: GPL-2.0+

// Computer Hardware IDs for multiple boards
// Generated from source_path

/ {
\tchid: chid {};
};

&chid {
"""

DTSI_FOOTER = """};
"""

# Constants for magic numbers
GUID_LENGTH = 36  # Length of GUID string without braces (8-4-4-4-12 format)
HWIDS_SECTION_HEADER_LINES = 2  # Lines for 'Hardware IDs' header and dashes
CHID_BINARY_LENGTH = 16  # Length of CHID binary data in bytes
HEX_BASE = 16  # Base for hexadecimal conversions

# Field types for special handling
VERSION_FIELDS = {
    'BiosMajorRelease', 'BiosMinorRelease',
    'FirmwareMajorRelease', 'FirmwareMinorRelease'
}
HEX_ENCLOSURE_FIELD = 'EnclosureKind'

class CHIDField(IntEnum):
    """CHID field types matching the C enum chid_field_t."""
    MANUF = 0
    FAMILY = 1
    PRODUCT_NAME = 2
    PRODUCT_SKU = 3
    BOARD_MANUF = 4
    BOARD_PRODUCT = 5
    BIOS_VENDOR = 6
    BIOS_VERSION = 7
    BIOS_MAJOR = 8
    BIOS_MINOR = 9
    ENCLOSURE_TYPE = 10


class CHIDVariant(IntEnum):
    """CHID variant IDs matching the Microsoft specification."""
    CHID_00 = 0   # Most specific
    CHID_01 = 1
    CHID_02 = 2
    CHID_03 = 3
    CHID_04 = 4
    CHID_05 = 5
    CHID_06 = 6
    CHID_07 = 7
    CHID_08 = 8
    CHID_09 = 9
    CHID_10 = 10
    CHID_11 = 11
    CHID_12 = 12
    CHID_13 = 13
    CHID_14 = 14  # Least specific


# Field mapping from HWIDS file field names to CHIDField bits and devicetree
# properties
# Format: 'HWIDSFieldName': (CHIDField.BIT or None, 'devicetree-property-name')
# Note: Firmware fields don't map to CHIDField bits
FIELD_MAP = {
    'Manufacturer': (CHIDField.MANUF, 'manufacturer'),
    'Family': (CHIDField.FAMILY, 'family'),
    'ProductName': (CHIDField.PRODUCT_NAME, 'product-name'),
    'ProductSku': (CHIDField.PRODUCT_SKU, 'product-sku'),
    'BaseboardManufacturer': (CHIDField.BOARD_MANUF, 'baseboard-manufacturer'),
    'BaseboardProduct': (CHIDField.BOARD_PRODUCT, 'baseboard-product'),
    'BiosVendor': (CHIDField.BIOS_VENDOR, 'bios-vendor'),
    'BiosVersion': (CHIDField.BIOS_VERSION, 'bios-version'),
    'BiosMajorRelease': (CHIDField.BIOS_MAJOR, 'bios-major-release'),
    'BiosMinorRelease': (CHIDField.BIOS_MINOR, 'bios-minor-release'),
    'EnclosureKind': (CHIDField.ENCLOSURE_TYPE, 'enclosure-kind'),
    'FirmwareMajorRelease': (None, 'firmware-major-release'),
    'FirmwareMinorRelease': (None, 'firmware-minor-release'),
}


# CHID variants table matching the C code variants array
CHID_VARIANTS = {
    CHIDVariant.CHID_00: {
        'name': 'HardwareID-00',
        'fields': (1 << CHIDField.MANUF) | (1 << CHIDField.FAMILY) |
                  (1 << CHIDField.PRODUCT_NAME) | (1 << CHIDField.PRODUCT_SKU) |
                  (1 << CHIDField.BIOS_VENDOR) | (1 << CHIDField.BIOS_VERSION) |
                  (1 << CHIDField.BIOS_MAJOR) | (1 << CHIDField.BIOS_MINOR)
    },
    CHIDVariant.CHID_01: {
        'name': 'HardwareID-01',
        'fields': (1 << CHIDField.MANUF) | (1 << CHIDField.FAMILY) |
                  (1 << CHIDField.PRODUCT_NAME) | (1 << CHIDField.BIOS_VENDOR) |
                  (1 << CHIDField.BIOS_VERSION) | (1 << CHIDField.BIOS_MAJOR) |
                  (1 << CHIDField.BIOS_MINOR)
    },
    CHIDVariant.CHID_02: {
        'name': 'HardwareID-02',
        'fields': (1 << CHIDField.MANUF) | (1 << CHIDField.PRODUCT_NAME) |
                  (1 << CHIDField.BIOS_VENDOR) | (1 << CHIDField.BIOS_VERSION) |
                  (1 << CHIDField.BIOS_MAJOR) | (1 << CHIDField.BIOS_MINOR)
    },
    CHIDVariant.CHID_03: {
        'name': 'HardwareID-03',
        'fields': (1 << CHIDField.MANUF) | (1 << CHIDField.FAMILY) |
                  (1 << CHIDField.PRODUCT_NAME) | (1 << CHIDField.PRODUCT_SKU) |
                  (1 << CHIDField.BOARD_MANUF) | (1 << CHIDField.BOARD_PRODUCT)
    },
    CHIDVariant.CHID_04: {
        'name': 'HardwareID-04',
        'fields': (1 << CHIDField.MANUF) | (1 << CHIDField.FAMILY) |
                  (1 << CHIDField.PRODUCT_NAME) | (1 << CHIDField.PRODUCT_SKU)
    },
    CHIDVariant.CHID_05: {
        'name': 'HardwareID-05',
        'fields': (1 << CHIDField.MANUF) | (1 << CHIDField.FAMILY) |
                  (1 << CHIDField.PRODUCT_NAME)
    },
    CHIDVariant.CHID_06: {
        'name': 'HardwareID-06',
        'fields': (1 << CHIDField.MANUF) | (1 << CHIDField.PRODUCT_SKU) |
                  (1 << CHIDField.BOARD_MANUF) | (1 << CHIDField.BOARD_PRODUCT)
    },
    CHIDVariant.CHID_07: {
        'name': 'HardwareID-07',
        'fields': (1 << CHIDField.MANUF) | (1 << CHIDField.PRODUCT_SKU)
    },
    CHIDVariant.CHID_08: {
        'name': 'HardwareID-08',
        'fields': (1 << CHIDField.MANUF) | (1 << CHIDField.PRODUCT_NAME) |
                  (1 << CHIDField.BOARD_MANUF) | (1 << CHIDField.BOARD_PRODUCT)
    },
    CHIDVariant.CHID_09: {
        'name': 'HardwareID-09',
        'fields': (1 << CHIDField.MANUF) | (1 << CHIDField.PRODUCT_NAME)
    },
    CHIDVariant.CHID_10: {
        'name': 'HardwareID-10',
        'fields': (1 << CHIDField.MANUF) | (1 << CHIDField.FAMILY) |
                  (1 << CHIDField.BOARD_MANUF) | (1 << CHIDField.BOARD_PRODUCT)
    },
    CHIDVariant.CHID_11: {
        'name': 'HardwareID-11',
        'fields': (1 << CHIDField.MANUF) | (1 << CHIDField.FAMILY)
    },
    CHIDVariant.CHID_12: {
        'name': 'HardwareID-12',
        'fields': (1 << CHIDField.MANUF) | (1 << CHIDField.ENCLOSURE_TYPE)
    },
    CHIDVariant.CHID_13: {
        'name': 'HardwareID-13',
        'fields': (1 << CHIDField.MANUF) | (1 << CHIDField.BOARD_MANUF) |
                  (1 << CHIDField.BOARD_PRODUCT)
    },
    CHIDVariant.CHID_14: {
        'name': 'HardwareID-14',
        'fields': (1 << CHIDField.MANUF)
    }
}


def load_compatible_map(hwids_dir):
    """Load the compatible string mapping from compatible-map file

    Args:
        hwids_dir (str): Directory containing the compatible-map file

    Returns:
        dict: Mapping from filename to compatible string, empty if there is no
            map
    """
    compatible_map = {}
    map_file = os.path.join(hwids_dir, 'compatible-map')

    if not os.path.exists(map_file):
        return compatible_map

    with open(map_file, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith('#'):
                parts = line.split(': ', 1)
                if len(parts) == 2:
                    compatible_map[parts[0]] = parts[1]

    return compatible_map


def parse_variant_description(description):
    """Parse variant description to determine CHID variant ID and fields mask

    Args:
        description (str): Description text after '<-' in HWIDS file

    Returns:
        tuple: (variant_id, fields_mask) where variant_id is int (0-14) or None,
               and fields_mask is the bitmask or None if not parseable

    Examples:
        >>> parse_variant_description('Manufacturer + Family + ProductName')
        (5, 7)  # HardwareID-05 with fields 0x1|0x2|0x4 = 0x7

        >>> parse_variant_description('Manufacturer')
        (14, 1)  # HardwareID-14 with field 0x1

        >>> parse_variant_description('Invalid + Field')
        (None, 0)  # Unknown fields, no variant match
    """
    # Parse field names and match to variants
    desc = description.strip()

    # Parse the field list
    fields_mask = 0
    field_parts = desc.split(' + ')
    for part in field_parts:
        part = part.strip()
        if part in FIELD_MAP:
            # Get CHIDField, ignore dt property name
            chid_field, _ = FIELD_MAP[part]
            if chid_field is not None:  # Only add to mask if it's a CHIDField
                fields_mask |= (1 << chid_field)

    # If no fields were parsed, return None for both
    if not fields_mask:
        return None, None

    # Match against known variants
    for variant_id, variant_info in CHID_VARIANTS.items():
        if variant_info['fields'] == fields_mask:
            return int(variant_id), fields_mask

    # Fields were parsed but don't match a known variant
    return None, fields_mask


def _parse_hardware_ids_section(content, filepath):
    """Parse the Hardware IDs section from HWIDS file content

    Args:
        content (str): Full file content
        filepath (str): Path to the file for error reporting

    Returns:
        list: List of (guid_string, variant_id, bitmask) tuples
    """
    hardware_ids = []
    ids_pattern = r'Hardware IDs\n-+\n(.*?)(?:\n\n|$)'
    ids_section = re.search(ids_pattern, content, re.DOTALL)
    if not ids_section:
        raise ValueError(f'{filepath}: Missing "Hardware IDs" section')

    for linenum, line in enumerate(ids_section.group(1).strip().split('\n'), 1):
        if not line.strip():
            continue

        # Extract GUID and variant info from line like:
        # '{810e34c6-cc69-5e36-8675-2f6e354272d3}' <- HardwareID-00
        guid_match = re.search(rf'\{{([0-9a-f-]{{{GUID_LENGTH}}})\}}', line)
        if not guid_match:
            continue

        guid = guid_match.group(1)

        # The '<-' separator is required for valid HWIDS files
        if '<-' not in line:
            # Calculate actual line number in file
            # (need to account for lines before Hardware IDs section)
            before = content[:content.find('Hardware IDs')].count('\n')
            # +2 for header and dashes
            actual_line = before + linenum + HWIDS_SECTION_HEADER_LINES
            raise ValueError(
                f"{filepath}:{actual_line}: Missing '<-' separator in "
                f'Hardware ID line: {line.strip()}')

        description = line.split('<-', 1)[1].strip()
        variant_id, bitmask = parse_variant_description(description)

        hardware_ids.append((guid, variant_id, bitmask))

    return hardware_ids


def parse_hwids_file(filepath):
    """Parse a HWIDS txt file and return computer info and hardware IDs

    Args:
        filepath (str): Path to the HWIDS txt file

    Returns:
        tuple: (computer_info dict, hardware_ids list of tuples)
                hardware_ids contains (guid_string, variant_id, bitmask) tuples
    """
    info = {}
    hardware_ids = []

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Extract computer information section
    info_section = re.search(r'Computer Information\n-+\n(.*?)\nHardware IDs',
                             content, re.DOTALL)
    if not info_section:
        raise ValueError(f'{filepath}: Missing "Computer Information" section')

    for line in info_section.group(1).strip().split('\n'):
        if ':' in line:
            key, value = line.split(':', 1)
            info[key.strip()] = value.strip()

    # Extract hardware IDs with variant information
    hardware_ids = _parse_hardware_ids_section(content, filepath)

    return info, hardware_ids


def _add_header(out, basename, source_path=None):
    """Add header section to devicetree source

    Args:
        out (StringIO): StringIO object to write to
        basename (str): Base filename for the header comment
        source_path (str): Path to the source file or directory
    """
    out.write('// SPDX-License-Identifier: GPL-2.0+\n')
    out.write('\n')
    out.write(f'// Computer Hardware IDs for {basename}\n')
    if source_path:
        out.write(f'// Generated from {source_path}\n')
    else:
        out.write('// Generated from board/efi/hwids/\n')
    out.write('\n')


def _add_computer_info(out, computer_info, indent=2):
    """Add computer information properties to devicetree source

    Args:
        out (StringIO): StringIO object to write to
        computer_info (dict): Dictionary of computer information fields
        indent (int): Number of tab indentations (default 2 for &chid
            structure)
    """
    indent = '\t' * indent
    out.write(f'{indent}// SMBIOS Computer Information\n')
    for key, value in computer_info.items():
        # Look up the devicetree property name from FIELD_MAP
        if key in FIELD_MAP:
            _, prop_name = FIELD_MAP[key]
        else:
            # Fallback for fields not in FIELD_MAP (e.g. FirmwareMajorRelease,
            # FirmwareMinorRelease)
            prop_name = key.lower().replace('release', '-release')

        # Handle numeric values vs strings
        if key in VERSION_FIELDS and value.isdigit():
            out.write(f'{indent}{prop_name} = <{value}>;\n')
        elif key == HEX_ENCLOSURE_FIELD:
            # Value is already a hex string, convert directly
            hex_val = int(value, HEX_BASE)
            out.write(f'{indent}{prop_name} = <0x{hex_val:x}>;\n')
        else:
            out.write(f'{indent}{prop_name} = "{value}";\n')


def _add_hardware_ids(out, hardware_ids, indent=2):
    """Add hardware IDs as subnodes to devicetree source

    Args:
        out (StringIO): StringIO object to write to
        hardware_ids (list): List of (guid_string, variant_id, bitmask) tuples
        indent (int): Number of tab indentations (default 2 for &chid
            structure)
    """
    indent = '\t' * indent
    out.write(f'{indent}// Hardware IDs (CHIDs)\n')

    extra_counter = 0
    for guid, variant_id, bitmask in hardware_ids:
        # Convert GUID string to binary array for devicetree
        guid_obj = uuid.UUID(guid)
        binary_data = guid_obj.bytes  # Raw 16 bytes, no endian conversion
        hex_bytes = ' '.join(f'{b:02x}' for b in binary_data)
        hex_array = f'[{hex_bytes}]'

        # Create node name - use variant number if available, otherwise extra-N
        if variant_id is not None:
            node_name = f'hardware-id-{variant_id:02d}'
            variant_info = CHID_VARIANTS.get(variant_id, {})
            comment = variant_info.get('name', f'Unknown-{variant_id}')
        else:
            node_name = f'extra-{extra_counter}'
            comment = 'unknown variant'
            extra_counter += 1

        out.write('\n')
        out.write(f'{indent}{node_name} {{ // {comment}\n')

        # Add variant property if known
        if variant_id is not None:
            out.write(f'{indent}\tvariant = <{variant_id}>;\n')

        # Add fields property if bitmask is known
        if bitmask is not None:
            out.write(f'{indent}\tfields = <0x{bitmask:x}>;\n')

        # Add CHID bytes
        out.write(f'{indent}\tchid = {hex_array};\n')

        out.write(f'{indent}}};\n')


def generate_dtsi(basename, compatible, computer_info, hardware_ids,
                   source_path=None):
    """Generate devicetree source content

    Args:
        basename (str): Base filename for comments and node name
        compatible (str): Compatible string for the node
        computer_info (dict): Dictionary of computer information
        hardware_ids (list): List of (guid_string, variant_id, bitmask) tuples
        source_path (str): Path to the source file or directory

    Returns:
        str: Complete devicetree source content

    Examples:
        >>> info = {'Manufacturer': 'ACME', 'ProductName': 'Device'}
        >>> hwids = [('12345678-1234-5678-9abc-123456789abc', 14, 1)]
        >>> dtsi = generate_dtsi('acme-device', 'acme,device', info, hwids)
        >>> '// Computer Hardware IDs for acme-device' in dtsi
        True
        >>> 'compatible = "acme,device"' in dtsi
        True
    """
    out = StringIO()

    _add_header(out, basename, source_path)

    # Start root node with chid declaration
    out.write('/ {\n')
    out.write('\tchid: chid {};\n')
    out.write('};\n')
    out.write('\n')

    # Add device content to chid node using reference
    out.write('&chid {\n')
    node_name = basename.replace('.', '-')
    out.write(f'\t{node_name} {{\n')
    out.write(f'\t\tcompatible = "{compatible}";\n')
    out.write('\n')

    _add_computer_info(out, computer_info, indent=2)
    out.write('\n')

    _add_hardware_ids(out, hardware_ids, indent=2)

    out.write('\t};\n')
    out.write('};\n')

    return out.getvalue()


def parse_arguments():
    """Parse command line arguments

    Returns:
        argparse.Namespace: Parsed command line arguments
    """
    parser = argparse.ArgumentParser(
        description='Convert HWIDS txt files to devicetree source (.dtsi)'
    )
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        'input_file',
        nargs='?',
        help='Path to HWIDS txt file (e.g., board/efi/hwids/filename.txt)'
    )
    group.add_argument(
        '-m', '--map-file',
        help='compatible.hwidmap file (processes all .txt files in same dir)'
    )
    parser.add_argument(
        '-o', '--output',
        help='Output file (default: basename.dtsi or hwids.dtsi for dir mode)'
    )
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='Show verbose output with conversion details'
    )
    parser.add_argument(
        '-D', '--debug',
        action='store_true',
        help='Show debug traceback on errors'
    )

    return parser.parse_args()


def _process_board_file(out, compatible, txt_file, boards_processed, verbose):
    """Process a single board HWIDS file and add to output

    Args:
        out (StringIO): StringIO object to write to
        compatible (str): Compatible string for the board
        txt_file (str): Full path to the HWIDS txt file
        boards_processed (int): Number of boards processed so far
        verbose (bool): Whether to show verbose output

    Returns:
        bool: True if board was successfully processed, False otherwise
    """
    basename = os.path.splitext(os.path.basename(txt_file))[0]
    if verbose:
        print(f'Processing {basename}...')

    computer, hardware_ids = parse_hwids_file(txt_file)

    if not hardware_ids:
        if verbose:
            print(f'  Warning: No hardware IDs found in {basename}')
        return False

    # Add blank line between boards
    if boards_processed > 0:
        out.write('\n')

    # Generate board node directly (combining all boards)
    node_name = basename.replace('.', '-')
    out.write(f'\t{node_name} {{\n')
    out.write(f'\t\tcompatible = "{compatible}";\n')
    out.write('\n')

    # Add computer info and hardware IDs
    _add_computer_info(out, computer, indent=2)
    out.write('\n')

    _add_hardware_ids(out, hardware_ids, indent=2)

    out.write('\t};\n')

    if verbose:
        print(f'  Added {len(hardware_ids)} hardware IDs')

    return True


def _load_and_validate_map_file(map_file_path, verbose=False):
    """Load and validate compatible map file

    Args:
        map_file_path (str): Path to the compatible.hwidmap file
        verbose (bool): Show verbose output

    Returns:
        tuple: (hwids_dir, compatible_map)
    """
    # Get directory containing the map file
    hwids_dir = os.path.dirname(map_file_path)

    # Load compatible string mapping from the specified file
    compatible_map = {}
    if os.path.exists(map_file_path):
        with open(map_file_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith('#'):
                    parts = line.split(': ', 1)
                    if len(parts) == 2:
                        compatible_map[parts[0]] = parts[1]

    if not compatible_map:
        raise ValueError(f'No valid mappings found in {map_file_path}')

    # Find all .txt files in the same directory
    txt_files = glob.glob(os.path.join(hwids_dir, '*.txt'))
    txt_basenames = {os.path.splitext(os.path.basename(f))[0]
                     for f in txt_files}

    # Check for files in map that don't exist
    map_files = set(compatible_map.keys())
    missing_files = map_files - txt_basenames
    if missing_files:
        raise ValueError('Files in map but not found in directory: '
                         f"{', '.join(sorted(missing_files))}")

    # Check for files in directory that aren't in map
    extra_files = txt_basenames - map_files
    if extra_files:
        file_list = ', '.join(sorted(extra_files))
        raise ValueError(f'Files in directory but not in map: {file_list}')

    if verbose:
        print(f'Using compatible map: {map_file_path}')
        print(f'Processing {len(compatible_map)} HWIDs files from map')
        print()

    return hwids_dir, compatible_map


def _finalise_combined_dtsi(out, hwids_dir, processed, skipped, verbose):
    """Finalize the combined DTSI output with validation and reporting

    Args:
        out (StringIO): StringIO object containing the main content
        hwids_dir (str): Directory path for header generation
        processed (int): Number of successfully processed files
        skipped (list): List of skipped board names
        verbose (bool): Whether to show verbose output

    Returns:
        str: Final DTSI content with header and footer
    """
    if not processed:
        raise ValueError('No valid HWIDS files could be processed')

    if verbose:
        print(f'\nProcessed {processed} boards successfully')

    # Print warning about skipped boards
    if skipped:
        print(f'Warning: Skipped {len(skipped)} unmapped boards: '
              f"{', '.join(skipped)}")

    header = DTSI_HEADER.replace('source_path', hwids_dir)
    return ''.join([header, out.getvalue(), DTSI_FOOTER])


def process_map_file(map_file_path, verbose=False):
    """Process HWIDS files specified in the map file and generate combined DTSI

    Args:
        map_file_path (str): Path to the compatible.hwidmap file
        verbose (bool): Show verbose output

    Returns:
        str: Combined DTSI content for all boards
    """
    hwids_dir, compatible_map = _load_and_validate_map_file(map_file_path,
                                                            verbose)

    out = StringIO()
    processed = 0
    skipped = []
    for basename in sorted(compatible_map.keys()):
        compatible = compatible_map[basename]

        # Skip files with 'none' mapping
        if compatible == 'none':
            skipped.append(basename)
            if verbose:
                print(f'Skipping {basename} (mapping: none)')
            continue

        # Process this board file
        txt_file = os.path.join(hwids_dir, f'{basename}.txt')
        if _process_board_file(out, compatible, txt_file, processed, verbose):
            processed += 1

    return _finalise_combined_dtsi(out, hwids_dir, processed, skipped,
                                   verbose)


def handle_map_file_mode(args):
    """Handle map file mode processing

    Args:
        args (argparse.Namespace): Parsed command line arguments

    Returns:
        str: Generated DTSI content
    """
    if not os.path.exists(args.map_file):
        raise FileNotFoundError(f'Map file {args.map_file} not found')

    dtsi_content = process_map_file(args.map_file, args.verbose)
    outfile = args.output or 'hwids.dtsi'

    if args.verbose:
        print(f'Generated combined DTSI -> {outfile}')
        print()

    return dtsi_content


def handle_single_file_mode(args):
    """Handle single file mode processing

    Args:
        args (argparse.Namespace): Parsed command line arguments

    Returns:
        str: Generated DTSI content
    """
    if not args.input_file:
        raise ValueError('input_file is required when not using --map-file')

    if not os.path.exists(args.input_file):
        raise FileNotFoundError(f"File '{args.input_file}' not found")

    # Get the directory and basename
    hwids_dir = os.path.dirname(args.input_file)
    basename = os.path.splitext(os.path.basename(args.input_file))[0]

    # Load compatible string mapping
    compatible_map = load_compatible_map(hwids_dir)
    compatible = compatible_map.get(basename, f'unknown,{basename}')

    # Parse the input file
    info, hardware_ids = parse_hwids_file(args.input_file)

    if not hardware_ids and args.verbose:
        print(f"Warning: No hardware IDs found in '{args.input_file}'")

    # Generate devicetree source
    content = generate_dtsi(basename, compatible, info,
                                 hardware_ids, args.input_file)

    outfile = args.output or f'{basename}.dtsi'
    if args.verbose:
        print(f'Converting {args.input_file} -> {outfile}')
        print(f'Compatible: {compatible}')
        print(f'Computer info fields: {len(info)}')
        print(f'Hardware IDs: {len(hardware_ids)}')
        print()

    return content


def main():
    """Main function

    Returns:
        int: Exit code (0 for success, 1 for error)
    """
    args = parse_arguments()
    try:
        # Choose processing mode and handle
        if args.map_file:
            content = handle_map_file_mode(args)
            outfile = args.output or 'hwids.dtsi'
        else:
            content = handle_single_file_mode(args)
            basename = os.path.splitext(os.path.basename(args.input_file))[0]
            outfile = args.output or f'{basename}.dtsi'

        # Write to file if output specified, otherwise print to stdout
        if args.output:
            try:
                with open(outfile, 'w', encoding='utf-8') as f:
                    f.write(content)
                if args.verbose:
                    print(f'Written to {outfile}')
            except (IOError, OSError) as e:
                print(f'Error writing to {outfile}: {e}')
                return 1
        else:
            print(content, end='')

        return 0

    except (ValueError, FileNotFoundError, IOError, OSError) as e:
        if args.debug:
            traceback.print_exc()
        else:
            print(f'Error: {e}')
        return 1


if __name__ == '__main__':
    sys.exit(main())
