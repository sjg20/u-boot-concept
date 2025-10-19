// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025 Canonical Ltd
 *
 * Test for TKey uclass and emulator
 */

#include <dm.h>
#include <dm/test.h>
#include <test/test.h>
#include <test/ut.h>
#include <tkey.h>

/* Test that we can find a TKey device */
static int dm_test_tkey_find(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_assertok(uclass_first_device_err(UCLASS_TKEY, &dev));
	ut_assertnonnull(dev);

	return 0;
}
DM_TEST(dm_test_tkey_find, UTF_SCAN_FDT);

/* Test getting UDI from TKey */
static int dm_test_tkey_get_udi(struct unit_test_state *uts)
{
	u8 udi[TKEY_UDI_SIZE];
	struct udevice *dev;

	ut_assertok(uclass_first_device_err(UCLASS_TKEY, &dev));

	ut_assertok(tkey_get_udi(dev, udi));

	/* Verify emulator returns expected UDI */
	ut_asserteq(0xa0, udi[0]);
	ut_asserteq(0xa1, udi[1]);
	ut_asserteq(0xa2, udi[2]);
	ut_asserteq(0xa3, udi[3]);
	ut_asserteq(0xa4, udi[4]);
	ut_asserteq(0xa5, udi[5]);
	ut_asserteq(0xa6, udi[6]);
	ut_asserteq(0xa7, udi[7]);

	return 0;
}
DM_TEST(dm_test_tkey_get_udi, UTF_SCAN_FDT);

/* Test getting name and version from TKey */
static int dm_test_tkey_get_name_version(struct unit_test_state *uts)
{
	char name0[TKEY_NAME_SIZE], name1[TKEY_NAME_SIZE];
	struct udevice *dev;
	u32 version;

	ut_assertok(uclass_first_device_err(UCLASS_TKEY, &dev));

	/* Get name and version */
	ut_assertok(tkey_get_name_version(dev, name0, name1, &version));

	/* Verify emulator returns expected values */
	ut_asserteq_str("tk1 ", name0);
	ut_asserteq_str("mkdf", name1);
	ut_asserteq(4, version);

	return 0;
}
DM_TEST(dm_test_tkey_get_name_version, UTF_SCAN_FDT);

/* Test checking firmware mode */
static int dm_test_tkey_in_app_mode(struct unit_test_state *uts)
{
	struct udevice *dev;
	int ret;

	ut_assertok(uclass_first_device_err(UCLASS_TKEY, &dev));

	/* Check mode - should be in firmware mode initially */
	ret = tkey_in_app_mode(dev);
	ut_assert(ret >= 0);
	ut_asserteq(0, ret);  /* 0 = firmware mode */

	return 0;
}
DM_TEST(dm_test_tkey_in_app_mode, UTF_SCAN_FDT);

/* Test loading an app */
static int dm_test_tkey_load_app(struct unit_test_state *uts)
{
	struct udevice *dev;
	u8 dummy_app[128];
	int ret;

	ut_assertok(uclass_first_device_err(UCLASS_TKEY, &dev));

	/* Create a dummy app */
	memset(dummy_app, 0x42, sizeof(dummy_app));

	/* Load the app */
	ret = tkey_load_app(dev, dummy_app, sizeof(dummy_app));
	ut_assertok(ret);

	/* After loading, should be in app mode */
	ret = tkey_in_app_mode(dev);
	ut_assert(ret >= 0);
	ut_asserteq(1, ret);  /* 1 = app mode */

	return 0;
}
DM_TEST(dm_test_tkey_load_app, UTF_SCAN_FDT);

/* Test getting public key from signer app */
static int dm_test_tkey_get_pubkey(struct unit_test_state *uts)
{
	u8 pubkey[TKEY_PUBKEY_SIZE];
	struct udevice *dev;
	u8 dummy_app[128];
	int i;

	ut_assertok(uclass_first_device_err(UCLASS_TKEY, &dev));

	/* Load a dummy app first */
	memset(dummy_app, 0x42, sizeof(dummy_app));
	ut_assertok(tkey_load_app(dev, dummy_app, sizeof(dummy_app)));

	/* Get public key */
	ut_assertok(tkey_get_pubkey(dev, pubkey));

	/* Verify emulator returns expected pattern */
	for (i = 0; i < TKEY_PUBKEY_SIZE; i++)
		ut_asserteq(0x50 + (i & 0xf), pubkey[i]);

	return 0;
}
DM_TEST(dm_test_tkey_get_pubkey, UTF_SCAN_FDT);

/* Test deriving wrapping key from password */
static int dm_test_tkey_derive_wrapping_key(struct unit_test_state *uts)
{
	u8 wrapping_key[TKEY_WRAPPING_KEY_SIZE];
	const char *password = "test_password";
	/* Expected BLAKE2b(UDI || password) where UDI = a0a1a2a3a4a5a6a7 */
	const u8 expected[TKEY_WRAPPING_KEY_SIZE] = {
		0x95, 0x22, 0x9c, 0xd3, 0x76, 0x89, 0x8f, 0x3f,
		0xb0, 0x22, 0xa6, 0x27, 0x34, 0x9d, 0xc9, 0x85,
		0xbc, 0x46, 0x75, 0xda, 0x58, 0x0d, 0x26, 0x96,
		0xbd, 0xd6, 0xf7, 0x1f, 0x48, 0x8e, 0x30, 0x6c,
	};
	struct udevice *dev;

	ut_assertok(uclass_first_device_err(UCLASS_TKEY, &dev));

	/* Derive wrapping key from password */
	ut_assertok(tkey_derive_wrapping_key(dev, password, wrapping_key));

	/* Verify the exact wrapping key value */
	ut_asserteq_mem(expected, wrapping_key, TKEY_WRAPPING_KEY_SIZE);

	return 0;
}
DM_TEST(dm_test_tkey_derive_wrapping_key, UTF_SCAN_FDT);

/* Test deriving disk key with USS */
static int dm_test_tkey_derive_disk_key(struct unit_test_state *uts)
{
	const char *uss = "user_secret";
	u8 disk_key[TKEY_DISK_KEY_SIZE];
	u8 pubkey[TKEY_PUBKEY_SIZE];
	u8 key_hash[TKEY_HASH_SIZE];
	/* Expected pubkey from emulator (deterministic pattern) */
	const u8 expected_pubkey[TKEY_PUBKEY_SIZE] = {
		0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
		0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
		0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
		0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	};
	/* Expected disk key: BLAKE2b(pubkey) */
	const u8 expected_disk_key[TKEY_DISK_KEY_SIZE] = {
		0x22, 0x8b, 0x2f, 0x6a, 0xbf, 0x8b, 0xe0, 0x56,
		0x49, 0xb2, 0x41, 0x75, 0x86, 0x15, 0x0b, 0xbf,
		0x3e, 0x1b, 0x3f, 0x66, 0x9a, 0xfa, 0x1c, 0x61,
		0x51, 0xdd, 0xc7, 0x29, 0x57, 0x93, 0x3c, 0x21,
	};
	/* Expected key hash: BLAKE2b(disk_key) */
	const u8 expected_key_hash[TKEY_HASH_SIZE] = {
		0xa7, 0x2a, 0x46, 0xb8, 0xf8, 0xc7, 0xff, 0x08,
		0x24, 0x41, 0x6a, 0xda, 0x88, 0x6f, 0x62, 0xb6,
		0xc2, 0x80, 0x88, 0x96, 0xd7, 0x12, 0x01, 0xa3,
		0x28, 0x14, 0xab, 0x43, 0x2c, 0x7a, 0x81, 0xcf,
	};
	struct udevice *dev;
	u8 dummy_app[128];

	ut_assertok(uclass_first_device_err(UCLASS_TKEY, &dev));

	/* Create a dummy signer app */
	memset(dummy_app, 0x42, sizeof(dummy_app));

	/* Derive disk key */
	ut_assertok(tkey_derive_disk_key(dev, dummy_app, sizeof(dummy_app),
					 uss, strlen(uss), disk_key, pubkey,
					 key_hash));

	ut_asserteq_mem(expected_pubkey, pubkey, TKEY_PUBKEY_SIZE);
	ut_asserteq_mem(expected_disk_key, disk_key, TKEY_DISK_KEY_SIZE);
	ut_asserteq_mem(expected_key_hash, key_hash, TKEY_HASH_SIZE);

	return 0;
}
DM_TEST(dm_test_tkey_derive_disk_key, UTF_SCAN_FDT);

/* Test UDI not available in app mode */
static int dm_test_tkey_udi_app_mode(struct unit_test_state *uts)
{
	u8 udi[TKEY_UDI_SIZE];
	struct udevice *dev;
	u8 dummy_app[128];

	ut_assertok(uclass_first_device_err(UCLASS_TKEY, &dev));

	/* Load an app to enter app mode */
	memset(dummy_app, 0x42, sizeof(dummy_app));
	ut_assertok(tkey_load_app(dev, dummy_app, sizeof(dummy_app)));

	/* Verify we're in app mode */
	ut_asserteq(1, tkey_in_app_mode(dev));

	/* Try to get UDI - emulator returns -EIO for empty response */
	ut_asserteq(-EIO, tkey_get_udi(dev, udi));

	return 0;
}
DM_TEST(dm_test_tkey_udi_app_mode, UTF_SCAN_FDT);

/* Test loading app with USS */
static int dm_test_tkey_load_app_with_uss(struct unit_test_state *uts)
{
	struct udevice *dev;
	u8 dummy_app[128];
	const char *uss = "my_secret";

	ut_assertok(uclass_first_device_err(UCLASS_TKEY, &dev));

	/* Create a dummy app */
	memset(dummy_app, 0x55, sizeof(dummy_app));

	/* Load app with USS */
	ut_assertok(tkey_load_app_with_uss(dev, dummy_app, sizeof(dummy_app),
					   uss, strlen(uss)));

	/* Should be in app mode */
	ut_asserteq(1, tkey_in_app_mode(dev));

	return 0;
}
DM_TEST(dm_test_tkey_load_app_with_uss, UTF_SCAN_FDT);

/* Test basic read/write operations */
static int dm_test_tkey_read_write(struct unit_test_state *uts)
{
	/* Expected USB response: 0x52 0x02 [tk1 ] [mkdf] [version=4] */
	static const u8 expected[14] = {
		0x52, 0x02,		/* USB marker and response type */
		't', 'k', '1', ' ',	/* name0 */
		'm', 'k', 'd', 'f',	/* name1 */
		0x04, 0x00, 0x00, 0x00,	/* version = 4 (little-endian) */
	};
	struct udevice *dev;
	u8 write_buf[129];  /* Header + command */
	u8 read_buf[256];

	ut_assertok(uclass_first_device_err(UCLASS_TKEY, &dev));

	/* Prepare a GET_NAME_VERSION command */
	write_buf[0] = 0x10;  /* Header: CMD, FIRMWARE endpoint */
	write_buf[1] = 0x01;  /* CMD_GET_NAME_VERSION */

	/* Write the command - should return 2 bytes written */
	ut_asserteq(2, tkey_write(dev, write_buf, 2));

	/* Read the response - should get exactly 14 bytes */
	ut_asserteq(14, tkey_read_all(dev, read_buf, sizeof(read_buf), 1000));

	/* Verify full response matches expected */
	ut_asserteq_mem(expected, read_buf, 14);

	return 0;
}
DM_TEST(dm_test_tkey_read_write, UTF_SCAN_FDT);
