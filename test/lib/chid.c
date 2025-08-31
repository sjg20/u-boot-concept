// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for Computer Hardware Identifiers (Windows CHID) support
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <chid.h>
#include <test/lib.h>
#include <test/test.h>
#include <test/ut.h>
#include <u-boot/uuid.h>
#include <string.h>

static int chid_basic(struct unit_test_state *uts)
{
	struct chid_data data = {
		.manuf = "Test Manufacturer",
		.product_name = "Test Product",
		.family = "Test Family",
		.product_sku = "Test SKU",
		.bios_vendor = "Test BIOS Vendor",
		.bios_version = "1.0.0",
		.bios_major = 1,
		.bios_minor = 0,
	};
	u8 chid[UUID_LEN];

	/* Test HardwareID-00 (most specific) */
	ut_assertok(chid_generate(CHID_00, &data, chid));

	/* The CHID should not be all zeros */
	u8 zero_chid[UUID_LEN] = {0};
	ut_assert(memcmp(chid, zero_chid, UUID_LEN));

	return 0;
}
LIB_TEST(chid_basic, 0);

static int chid_variants(struct unit_test_state *uts)
{
	struct chid_data data = {
		.manuf = "Dell Inc.",
		.product_name = "OptiPlex 7090",
		.family = "OptiPlex",
		.product_sku = "0A5C",
		.bios_vendor = "Dell Inc.",
		.bios_version = "1.12.0",
		.bios_major = 1,
		.bios_minor = 12,
		.enclosure_type = 3,
	};
	u8 chid0[UUID_LEN], chid1[UUID_LEN], chid14[UUID_LEN];

	/* Test different variants produce different CHIDs */
	ut_assertok(chid_generate(CHID_00, &data, chid0));
	ut_assertok(chid_generate(CHID_01, &data, chid1));
	ut_assertok(chid_generate(CHID_14, &data, chid14));

	/* All CHIDs should be different */
	ut_assert(memcmp(chid0, chid1, UUID_LEN));
	ut_assert(memcmp(chid0, chid14, UUID_LEN));
	ut_assert(memcmp(chid1, chid14, UUID_LEN));

	return 0;
}
LIB_TEST(chid_variants, 0);

static int chid_missing_fields(struct unit_test_state *uts)
{
	struct chid_data data = {
		.manuf = "Test Manufacturer",
		/* Missing other fields */
	};
	struct chid_data empty_data = {0};
	u8 chid[UUID_LEN];

	/* Test HardwareID-14 (manufacturer only) should work */
	ut_assertok(chid_generate(CHID_14, &data, chid));

	/*
	 * Test HardwareID-05 (requires string fields only) with completely
	 * empty data should fail
	 */
	ut_asserteq(-ENODATA, chid_generate(CHID_05, &empty_data, chid));

	/* Test HardwareID-14 with empty data should also fail */
	ut_asserteq(-ENODATA, chid_generate(CHID_14, &empty_data, chid));

	return 0;
}
LIB_TEST(chid_missing_fields, 0);

static int chid_invalid_params(struct unit_test_state *uts)
{
	struct chid_data data = {
		.manuf = "Test Manufacturer",
	};
	u8 chid[UUID_LEN];

	/* Test invalid variant number */
	ut_asserteq(-EINVAL, chid_generate(-1, &data, chid));
	ut_asserteq(-EINVAL, chid_generate(15, &data, chid));

	/* Test NULL data */
	ut_asserteq(-EINVAL, chid_generate(CHID_00, NULL, chid));

	/* Test NULL chid output buffer */
	ut_asserteq(-EINVAL, chid_generate(CHID_00, &data, NULL));

	return 0;
}
LIB_TEST(chid_invalid_params, 0);

static int chid_consistent(struct unit_test_state *uts)
{
	struct chid_data data = {
		.manuf = "ACME Corp",
		.product_name = "Widget Pro",
		.bios_vendor = "ACME BIOS",
		.bios_version = "2.1.0",
		.bios_major = 2,
		.bios_minor = 1,
	};
	u8 chid1[UUID_LEN], chid2[UUID_LEN];
	char chid1_str[UUID_STR_LEN + 1], chid2_str[UUID_STR_LEN + 1];

	/* Generate the same CHID twice - should be identical */
	ut_assertok(chid_generate(CHID_02, &data, chid1));
	ut_assertok(chid_generate(CHID_02, &data, chid2));

	/* CHIDs should be identical for same input */
	uuid_bin_to_str(chid1, chid1_str, UUID_STR_FORMAT_STD);
	uuid_bin_to_str(chid2, chid2_str, UUID_STR_FORMAT_STD);
	ut_asserteq_str(chid1_str, chid2_str);

	return 0;
}
LIB_TEST(chid_consistent, 0);

static int chid_numeric(struct unit_test_state *uts)
{
	struct chid_data data = {
		.manuf = "Test Corp",
		.bios_major = 255,
		.bios_minor = 127,
		.enclosure_type = 99,
	};
	u8 zero_chid[UUID_LEN] = {0};
	u8 chid[UUID_LEN];

	/* Test with numeric fields only (manufacturer + numeric values) */
	/* HardwareID-12: Manufacturer + Enclosure Type */
	ut_assertok(chid_generate(CHID_12, &data, chid));

	/* CHID should be generated successfully */
	ut_assert(memcmp(chid, zero_chid, UUID_LEN));

	return 0;
}
LIB_TEST(chid_numeric, 0);

static int chid_real(struct unit_test_state *uts)
{
	/*
	 * Real data from Lenovo ThinkPad X13s Gen 1 (21BXCTO1WW)
	 * Test against actual CHIDs from Microsoft's ComputerHardwareIds.exe
	 * output
	 */
	struct chid_data data = {
		.manuf = "LENOVO",
		.family = "ThinkPad X13s Gen 1",
		.product_name = "21BXCTO1WW",
		.product_sku = "LENOVO_MT_21BX_BU_Think_FM_ThinkPad X13s Gen 1",
		.board_manuf = "LENOVO",
		.board_product = "21BXCTO1WW",
		.bios_vendor = "LENOVO",
		.bios_version = "N3HET88W (1.60 )",
		.bios_major = 1,
		.bios_minor = 60,
		.enclosure_type = 0x0a,
	};
	u8 chid[UUID_LEN];
	char chid_str[UUID_STR_LEN + 1];

	/* Test HardwareID-14 (Manufacturer only) */
	ut_assertok(chid_generate(CHID_14, &data, chid));
	uuid_bin_to_str(chid, chid_str, UUID_STR_FORMAT_STD);
	ut_asserteq_str("6de5d951-d755-576b-bd09-c5cf66b27234", chid_str);

	/* Test HardwareID-11 (Manufacturer + Family) */
	ut_assertok(chid_generate(CHID_11, &data, chid));
	uuid_bin_to_str(chid, chid_str, UUID_STR_FORMAT_STD);
	ut_asserteq_str("f249803d-0d95-54f3-a28f-f26c14a03f3b", chid_str);

	/* Test HardwareID-12 (Manufacturer + EnclosureKind) */
	ut_assertok(chid_generate(CHID_12, &data, chid));
	uuid_bin_to_str(chid, chid_str, UUID_STR_FORMAT_STD);
	ut_asserteq_str("5e820764-888e-529d-a6f9-dfd12bacb160", chid_str);

	/*
	 * Test HardwareID-13 (Manufacturer + BaseboardManufacturer +
	 * BaseboardProduct)
	 */
	ut_assertok(chid_generate(CHID_13, &data, chid));
	uuid_bin_to_str(chid, chid_str, UUID_STR_FORMAT_STD);
	ut_asserteq_str("156c9b34-bedb-5bfd-ae1f-ef5d2a994967", chid_str);

	return 0;
}
LIB_TEST(chid_real, 0);

static int chid_exact(struct unit_test_state *uts)
{
	/*
	 * Test exact CHID matching against Microsoft's ComputerHardwareIds.exe
	 * Using Lenovo ThinkPad X13s Gen 1 data from reference file
	 * Expected CHID for HardwareID-14 (Manufacturer only):
	 * {6de5d951-d755-576b-bd09-c5cf66b27234}
	 */
	struct chid_data data = {
		.manuf = "LENOVO",
		.family = "ThinkPad X13s Gen 1",
		.product_name = "21BXCTO1WW",
		.product_sku = "LENOVO_MT_21BX_BU_Think_FM_ThinkPad X13s Gen 1",
		.board_manuf = "LENOVO",
		.board_product = "21BXCTO1WW",
		.bios_vendor = "LENOVO",
		.bios_version = "N3HET88W (1.60 )",
		.bios_major = 1,
		.bios_minor = 60,
		.enclosure_type = 0x0a,
	};
	char chid_str[UUID_STR_LEN + 1];
	u8 chid[UUID_LEN];

	/* Test HardwareID-14 (Manufacturer only) */
	ut_assertok(chid_generate(CHID_14, &data, chid));

	/* Convert CHID to string and compare with expected GUID string */
	uuid_bin_to_str(chid, chid_str, UUID_STR_FORMAT_STD);
	ut_asserteq_str("6de5d951-d755-576b-bd09-c5cf66b27234", chid_str);

	return 0;
}
LIB_TEST(chid_exact, 0);
