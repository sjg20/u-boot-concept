#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
"""
Test for hwids-to-dtsi.py script

Validates that the HWIDS to devicetree conversion script correctly parses
hardware ID files and generates proper devicetree-source output
"""

import os
import sys
import tempfile
import unittest
import uuid
from io import StringIO

# Add the scripts directory to the path
script_dir = os.path.join(os.path.dirname(__file__), '..', '..', 'scripts')
sys.path.insert(0, script_dir)

# pylint: disable=wrong-import-position,import-error
from hwids_to_dtsi import (
    load_compatible_map,
    parse_hwids_file,
    generate_dtsi,
    parse_variant_description,
    VERSION_FIELDS,
    HEX_ENCLOSURE_FIELD,
    _finalise_combined_dtsi
)


class TestHwidsToDeviceTree(unittest.TestCase):
    """Test cases for HWIDS to devicetree conversion"""

    def setUp(self):
        """Set up test fixtures"""
        self.test_hwids_content = """Computer Information
--------------------
BiosVendor: Insyde Corp.
BiosVersion: V1.24
BiosMajorRelease: 0
BiosMinorRelease: 0
FirmwareMajorRelease: 01
FirmwareMinorRelease: 15
Manufacturer: Acer
Family: Swift 14 AI
ProductName: Swift SF14-11
ProductSku:
EnclosureKind: a
BaseboardManufacturer: SX1
BaseboardProduct: Bluetang_SX1
Hardware IDs
------------
{27d2dba8-e6f1-5c19-ba1c-c25a4744c161}   <- Manufacturer + Family + ProductName + ProductSku + BiosVendor + BiosVersion + BiosMajorRelease + BiosMinorRelease
{676172cd-d185-53ed-aac6-245d0caa02c4}   <- Manufacturer + Family + ProductName + BiosVendor + BiosVersion + BiosMajorRelease + BiosMinorRelease
{20c2cf2f-231c-5d02-ae9b-c837ab5653ed}   <- Manufacturer + ProductName + BiosVendor + BiosVersion + BiosMajorRelease + BiosMinorRelease
{f2ea7095-999d-5e5b-8f2a-4b636a1e399f}   <- Manufacturer + Family + ProductName + ProductSku + BaseboardManufacturer + BaseboardProduct
{331d7526-8b88-5923-bf98-450cf3ea82a4}   <- Manufacturer + Family + ProductName + ProductSku
{98ad068a-f812-5f13-920c-3ff3d34d263f}   <- Manufacturer + Family + ProductName
{3f49141c-d8fb-5a6f-8b4a-074a2397874d}   <- Manufacturer + ProductSku + BaseboardManufacturer + BaseboardProduct
{7c107a7f-2d77-51aa-aef8-8d777e26ffbc}   <- Manufacturer + ProductSku
{6a12c9bc-bcfa-5448-9f66-4159dbe8c326}   <- Manufacturer + ProductName + BaseboardManufacturer + BaseboardProduct
{f55122fb-303f-58bc-b342-6ef653956d1d}   <- Manufacturer + ProductName
{ee8fa049-e5f4-51e4-89d8-89a0140b8f38}   <- Manufacturer + Family + BaseboardManufacturer + BaseboardProduct
{4cdff732-fd0c-5bac-b33e-9002788ea557}   <- Manufacturer + Family
{92dcc94d-48f7-5ee8-b9ec-a6393fb7a484}   <- Manufacturer + EnclosureKind
{32f83b0f-1fad-5be2-88be-5ab020e7a70e}   <- Manufacturer + BaseboardManufacturer + BaseboardProduct
{1e301734-5d49-5df4-9ed2-aa1c0a9dddda}   <- Manufacturer
Extra Hardware IDs
------------------
{058c0739-1843-5a10-bab7-fae8aaf30add}   <- Manufacturer + Family + ProductName + ProductSku + BiosVendor
{100917f4-9c0a-5ac3-a297-794222da9bc9}   <- Manufacturer + Family + ProductName + BiosVendor
{86654360-65f0-5935-bc87-81102c6a022b}   <- Manufacturer + BiosVendor
"""

        self.test_compatible_map = """# SPDX-License-Identifier: GPL-2.0+
# compatible map
test-device: test,example-device
"""

    def test_parse_hwids_file(self):
        """Test parsing of HWIDS file content"""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt',
                                         delete=False) as outf:
            outf.write(self.test_hwids_content)
            outf.flush()

            try:
                info, hardware_ids = parse_hwids_file(outf.name)
                expected_info = {
                    'BiosVendor': 'Insyde Corp.',
                    'BiosVersion': 'V1.24',
                    'BiosMajorRelease': '0',
                    'BiosMinorRelease': '0',
                    'FirmwareMajorRelease': '01',
                    'FirmwareMinorRelease': '15',
                    'Manufacturer': 'Acer',
                    'Family': 'Swift 14 AI',
                    'ProductName': 'Swift SF14-11',
                    'ProductSku': '',
                    'EnclosureKind': 'a',
                    'BaseboardManufacturer': 'SX1',
                    'BaseboardProduct': 'Bluetang_SX1'
                }
                self.assertEqual(info, expected_info)

                # Check hardware IDs (now tuples with variant info and bitmask)
                expected_ids = [
                    # Variant 0: All main fields
                    ('27d2dba8-e6f1-5c19-ba1c-c25a4744c161', 0, 0x3cf),
                    # Variant 1: Without SKU
                    ('676172cd-d185-53ed-aac6-245d0caa02c4', 1, 0x3c7),
                    # Variant 2: Without family
                    ('20c2cf2f-231c-5d02-ae9b-c837ab5653ed', 2, 0x3c5),
                    # Variant 3: With baseboard, no BIOS version
                    ('f2ea7095-999d-5e5b-8f2a-4b636a1e399f', 3, 0x3f),
                    # Variant 4: Basic product ID
                    ('331d7526-8b88-5923-bf98-450cf3ea82a4', 4, 0xf),
                    # Variant 5: Without SKU
                    ('98ad068a-f812-5f13-920c-3ff3d34d263f', 5, 0x7),
                    # Variant 6: SKU with baseboard
                    ('3f49141c-d8fb-5a6f-8b4a-074a2397874d', 6, 0x39),
                    # Variant 7: Manufacturer and SKU
                    ('7c107a7f-2d77-51aa-aef8-8d777e26ffbc', 7, 0x9),
                    # Variant 8: Product name with baseboard
                    ('6a12c9bc-bcfa-5448-9f66-4159dbe8c326', 8, 0x35),
                    # Variant 9: Manufacturer and product name
                    ('f55122fb-303f-58bc-b342-6ef653956d1d', 9, 0x5),
                    # Variant 10: Family with baseboard
                    ('ee8fa049-e5f4-51e4-89d8-89a0140b8f38', 10, 0x33),
                    # Variant 11: Manufacturer and family
                    ('4cdff732-fd0c-5bac-b33e-9002788ea557', 11, 0x3),
                    # Variant 12: Manufacturer and enclosure
                    ('92dcc94d-48f7-5ee8-b9ec-a6393fb7a484', 12, 0x401),
                    # Variant 13: Manufacturer with baseboard
                    ('32f83b0f-1fad-5be2-88be-5ab020e7a70e', 13, 0x31),
                    # Variant 14: Manufacturer only
                    ('1e301734-5d49-5df4-9ed2-aa1c0a9dddda', 14, 0x1),
                    # Extra Hardware IDs (non-standard variants)
                    # Extra: Manufacturer + Family + ProductName + ProductSku + BiosVendor
                    ('058c0739-1843-5a10-bab7-fae8aaf30add', None, 0x4f),
                    # Extra: Manufacturer + Family + ProductName + BiosVendor
                    ('100917f4-9c0a-5ac3-a297-794222da9bc9', None, 0x47),
                    # Extra: Manufacturer + BiosVendor
                    ('86654360-65f0-5935-bc87-81102c6a022b', None, 0x41),
                ]
                self.assertEqual(hardware_ids, expected_ids)

            finally:
                os.unlink(outf.name)

    def test_load_compatible_map(self):
        """Test loading compatible string mapping"""
        with tempfile.TemporaryDirectory() as tmpdir:
            map_file = os.path.join(tmpdir, 'compatible-map')
            with open(map_file, 'w', encoding='utf-8') as f:
                f.write(self.test_compatible_map)

            compatible_map = load_compatible_map(tmpdir)
            self.assertEqual(compatible_map['test-device'],
                             'test,example-device')

    def test_guid_to_binary(self):
        """Test GUID to binary conversion"""
        test_guid = '810e34c6-cc69-5e36-8675-2f6e354272d3'
        guid_obj = uuid.UUID(test_guid)
        binary_data = guid_obj.bytes

        # Should be 16 bytes
        self.assertEqual(len(binary_data), 16)

        # Test known conversion (raw bytes in string order)
        expected = bytearray([
            0x81, 0x0e, 0x34, 0xc6,  # time_low (raw bytes)
            0xcc, 0x69,              # time_mid (raw bytes)
            0x5e, 0x36,              # time_hi (raw bytes)
            0x86, 0x75,              # clock_seq (raw bytes)
            0x2f, 0x6e, 0x35, 0x42, 0x72, 0xd3  # node (raw bytes)
        ])
        self.assertEqual(binary_data, bytes(expected))


    def test_generate_dtsi(self):
        """Test devicetree source generation"""
        info = {
            'Manufacturer': 'LENOVO',
            'ProductName': '21BXCTO1WW',
            'BiosMajorRelease': '1',
            'EnclosureKind': 'a'
        }
        hardware_ids = [('810e34c6-cc69-5e36-8675-2f6e354272d3', 0, 0x3cf)]

        content = generate_dtsi('test-device', 'test,example-device',
                                     info, hardware_ids)

        self.assertIn('// SPDX-License-Identifier: GPL-2.0+', content)
        self.assertIn('test-device {', content)
        self.assertIn('compatible = "test,example-device";', content)
        self.assertIn('manufacturer = "LENOVO";', content)
        self.assertIn('product-name = "21BXCTO1WW";', content)
        self.assertIn('bios-major-release = <1>;', content)
        self.assertIn('enclosure-kind = <0xa>;', content)
        self.assertIn('// Hardware IDs (CHIDs)', content)
        self.assertIn('hardware-id-00 {', content)
        self.assertIn('variant = <0>;', content)
        self.assertIn('fields = <0x3cf>;', content)
        self.assertIn(
            'chid = [81 0e 34 c6 cc 69 5e 36 86 75 2f 6e 35 42 72 d3];',
            content)

    def test_invalid_guid_format(self):
        """Test error handling for invalid GUID format"""
        with self.assertRaises(ValueError):
            uuid.UUID('invalid-guid-format')

    def test_missing_compatible_map(self):
        """Test behavior when compatible-map file is missing"""
        with tempfile.TemporaryDirectory() as tmpdir:
            compatible_map = load_compatible_map(tmpdir)
            self.assertEqual(compatible_map, {})

    def test_enclosure_kind_conversion(self):
        """Test enclosure kind hex conversion"""
        info = {'EnclosureKind': 'a'}
        hardware_ids = []

        content = generate_dtsi('test', 'test,device', info, hardware_ids)
        self.assertIn('enclosure-kind = <0xa>;', content)

        # Test numeric enclosure kind ('10' is interpreted as hex 0x10)
        info = {'EnclosureKind': '10'}
        content = generate_dtsi('test', 'test,device', info, hardware_ids)
        self.assertIn('enclosure-kind = <0x10>;', content)

    def test_empty_hardware_ids(self):
        """Test handling of empty hardware IDs list"""
        info = {'Manufacturer': 'TEST'}
        hardware_ids = []

        content = generate_dtsi('test', 'test,device', info, hardware_ids)
        self.assertIn('// Hardware IDs (CHIDs)', content)

        # Should have no hardware-id-XX or extra-X nodes
        self.assertNotIn('hardware-id-', content)
        self.assertNotIn('extra-', content)

    def test_parse_variant_from_field_description(self):
        """Test parsing variant ID from field descriptions"""
        # Test variant 0 - most specific
        desc = ('Manufacturer + Family + ProductName + ProductSku + '
                'BiosVendor + BiosVersion + BiosMajorRelease + '
                'BiosMinorRelease')
        variant_id, fields_mask = parse_variant_description(desc)
        self.assertEqual(variant_id, 0)
        self.assertEqual(fields_mask, 0x3cf)

        # Test variant 14 - least specific (manufacturer only)
        desc = 'Manufacturer'
        variant_id, fields_mask = parse_variant_description(desc)
        self.assertEqual(variant_id, 14)
        self.assertEqual(fields_mask, 0x1)

        # Test variant 5 - manufacturer, family, product name
        desc = 'Manufacturer + Family + ProductName'
        variant_id, fields_mask = parse_variant_description(desc)
        self.assertEqual(variant_id, 5)
        self.assertEqual(fields_mask, 0x7)

    def test_constants_usage(self):
        """Test that magic number constants are used correctly"""
        # Test GUID_LENGTH constant in regex pattern
        test_guid = '12345678-1234-5678-9abc-123456789abc'
        content = f'''Computer Information
--------------------
Manufacturer: Test

Hardware IDs
------------
{{{test_guid}}}   <- Manufacturer
'''
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
            f.write(content)
            f.flush()
            try:
                _info, hardware_ids = parse_hwids_file(f.name)
                # Should successfully parse the GUID
                self.assertEqual(len(hardware_ids), 1)
                self.assertEqual(hardware_ids[0][0], test_guid)
            finally:
                os.unlink(f.name)

    def test_version_fields_constants(self):
        """Test that VERSION_FIELDS constant is used correctly"""

        # Test that all expected version fields are in the constant
        expected_fields = {'BiosMajorRelease', 'BiosMinorRelease',
                          'FirmwareMajorRelease', 'FirmwareMinorRelease'}
        self.assertTrue(expected_fields.issubset(VERSION_FIELDS))

        # Test HEX_ENCLOSURE_FIELD constant
        self.assertEqual(HEX_ENCLOSURE_FIELD, 'EnclosureKind')


if __name__ == '__main__':
    unittest.main()
