// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025 Canonical Ltd
 *
 * Command for communicating with Tillitis TKey to create wrapping keys
 * from user-provided passwords.
 */

#include <command.h>
#include <console.h>
#include <dm.h>
#include <hexdump.h>
#include <malloc.h>
#include <time.h>
#include <tkey.h>
#include <asm/unaligned.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/errno.h>

static struct udevice *tkey_get_device(void)
{
	struct udevice *dev;
	int ret;

	ret = uclass_first_device_err(UCLASS_TKEY, &dev);
	if (ret) {
		printf("No device found (err %dE)\n", ret);
		return NULL;
	}

	return dev;
}

static void print_hex(const char *label, const u8 *data, size_t len)
{
	size_t i;

	printf("%s: ", label);
	for (i = 0; i < len; i++)
		printf("%02x", data[i]);
	printf("\n");
}

static int do_tkey_connect(struct cmd_tbl *cmdtp, int flag, int argc,
			   char *const argv[])
{
	struct udevice *dev;

	dev = tkey_get_device();
	if (!dev)
		return CMD_RET_FAILURE;

	printf("Connected to TKey device\n");

	return CMD_RET_SUCCESS;
}

static int do_tkey_info(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	char name0[TKEY_NAME_SIZE], name1[TKEY_NAME_SIZE];
	u8 udi[TKEY_UDI_SIZE];
	struct udevice *dev;
	u32 version;
	int ret;

	dev = tkey_get_device();
	if (!dev)
		return CMD_RET_FAILURE;

	ret = tkey_get_name_version(dev, name0, name1, &version);
	if (ret) {
		printf("Failed to get device info (err %dE)\n", ret);
		return CMD_RET_FAILURE;
	}

	printf("Name0: %.4s Name1: %.4s Version: %u\n", name0, name1, version);

	ret = tkey_get_udi(dev, udi);
	if (ret) {
		if (ret == -ENOTSUPP)
			printf("UDI not available - replug device\n");
		else
			printf("Failed to get UDI (err %dE)\n", ret);
		return CMD_RET_FAILURE;
	}
	print_hex("UDI", udi, TKEY_UDI_SIZE);

	return CMD_RET_SUCCESS;
}

static int do_tkey_wrapkey(struct cmd_tbl *cmdtp, int flag, int argc,
			   char *const argv[])
{
	u8 wrapping_key[TKEY_WRAPPING_KEY_SIZE];
	const char *password;
	struct udevice *dev;
	int ret;

	if (argc != 2)
		return CMD_RET_USAGE;

	dev = tkey_get_device();
	if (!dev)
		return CMD_RET_FAILURE;

	password = argv[1];

	ret = tkey_derive_wrapping_key(dev, password, wrapping_key);
	if (ret) {
		if (ret == -ENOTSUPP)
			printf("UDI not available - replug device\n");
		else
			printf("Cannot derive wrapping key (err %dE)\n", ret);
		return CMD_RET_FAILURE;
	}

	print_hex("Wrapping Key", wrapping_key, TKEY_WRAPPING_KEY_SIZE);

	return CMD_RET_SUCCESS;
}

static int do_tkey_fwmode(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	struct udevice *dev;
	int ret;

	dev = tkey_get_device();
	if (!dev)
		return CMD_RET_FAILURE;

	ret = tkey_in_app_mode(dev);
	if (ret < 0) {
		printf("Failed to check device mode (err %dE)\n", ret);
		return CMD_RET_FAILURE;
	}

	if (!ret)
		printf("firmware mode\n");
	else
		printf("app mode\n");

	return CMD_RET_SUCCESS;
}

static int do_tkey_signer(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	printf("signer binary: %lx bytes at %p-%p\n", TKEY_SIGNER_SIZE,
	       __signer_1_0_0_begin, __signer_1_0_0_end);

	return CMD_RET_SUCCESS;
}

static int do_tkey_getkey(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	const char *hash = NULL;
	u8 expect[TKEY_HASH_SIZE];
	u8 disk_key[TKEY_DISK_KEY_SIZE];
	u8 key_hash[TKEY_HASH_SIZE];
	u8 pubkey[TKEY_PUBKEY_SIZE];
	bool verify = false;
	struct udevice *dev;
	u32 uss_len, ret;
	const char *uss;

	if (argc != 2 && argc != 3)
		return CMD_RET_USAGE;

	dev = tkey_get_device();
	if (!dev)
		return CMD_RET_FAILURE;

	uss = argv[1];
	uss_len = strlen(uss);
	if (uss_len > TKEY_USS_MAX_SIZE) {
		printf("USS too long (max %x bytes, got %x)\n",
		       TKEY_USS_MAX_SIZE, uss_len);
		return CMD_RET_FAILURE;
	}

	/* Check if verification hash is provided */
	if (argc == 3) {
		int i;

		hash = argv[2];
		verify = true;

		/* Convert hex string to bytes */
		if (strlen(hash) != TKEY_HASH_SIZE * 2) {
			printf("Verification hash must be %x hex chars\n",
			       TKEY_HASH_SIZE * 2);
			return CMD_RET_USAGE;
		}

		for (i = 0; i < TKEY_HASH_SIZE; i++)
			expect[i] = (hex_to_bin(hash[i * 2]) << 4) |
				     hex_to_bin(hash[i * 2 + 1]);
	}

	/* Derive disk key using uclass function */
	ret = tkey_derive_disk_key(dev, (const u8 *)__signer_1_0_0_begin,
				   TKEY_SIGNER_SIZE, (const u8 *)uss,
				   uss_len, disk_key, pubkey, key_hash);
	if (ret) {
		printf("Failed to derive disk key (err %dE)\n", ret);
		return CMD_RET_FAILURE;
	}

	/* Display results */
	print_hex("Public Key", pubkey, TKEY_PUBKEY_SIZE);
	print_hex("Disk Key", disk_key, TKEY_DISK_KEY_SIZE);

	/* Verify or display verification hash */
	if (verify) {
		/* Verify USS by comparing hashes */
		if (memcmp(key_hash, expect, TKEY_HASH_SIZE) == 0) {
			printf("\npassword correct\n");
		} else {
			printf("\nwrong password\n");
			print_hex("Expected", expect, TKEY_HASH_SIZE);
			print_hex("Got", key_hash, TKEY_HASH_SIZE);
			return CMD_RET_FAILURE;
		}
	} else {
		print_hex("Verification Hash", key_hash, TKEY_HASH_SIZE);
		/* to verify USS later: tkey getkey <uss> <verification-hash> */
	}

	return CMD_RET_SUCCESS;
}

static int do_tkey_loadapp(struct cmd_tbl *cmdtp, int flag, int argc,
			   char *const argv[])
{
	struct udevice *dev;
	const char *uss = NULL;
	u32 ret, ulen = 0;

	if (argc != 1 && argc != 2)
		return CMD_RET_USAGE;

	dev = tkey_get_device();
	if (!dev)
		return CMD_RET_FAILURE;

	/* Optional USS parameter */
	if (argc == 2) {
		uss = argv[1];
		ulen = strlen(uss);
		if (ulen > TKEY_USS_MAX_SIZE) {
			printf("USS too long (max %x bytes, got %x)\n",
			       TKEY_USS_MAX_SIZE, ulen);
			return CMD_RET_FAILURE;
		}
	}

	printf("Loading signer app (%lx bytes)%s...", TKEY_SIGNER_SIZE,
	       uss ? " with USS" : "");
	ret = tkey_load_app_with_uss(dev, (const u8 *)__signer_1_0_0_begin,
				     TKEY_SIGNER_SIZE, (const u8 *)uss, ulen);
	if (ret) {
		if (ret == -ENOTSUPP)
			printf("Invalid mode - replug device?\n");
		else
			printf("Failed to load app (err %dE)\n", ret);
		return CMD_RET_FAILURE;
	}
	printf("done\n");

	return CMD_RET_SUCCESS;
}

U_BOOT_LONGHELP(tkey,
	"connect    - Connect to TKey device\n"
	"tkey fwmode     - Check if device is in firmware or app mode\n"
	"tkey getkey <uss> [verify-hash] - Get disk encryption key\n"
	"    Loads app with USS, derives key. Same USS always produces same key.\n"
	"    Optional verify-hash checks if USS is correct\n"
	"tkey info       - Show TKey device information\n"
	"tkey loadapp [uss] - Load embedded signer app to TKey\n"
	"    Firmware mode only. Optional USS for key derivation\n"
	"tkey signer     - Show embedded signer binary information\n"
	"tkey wrapkey <password> - Create wrapping key from password and UDI");

U_BOOT_CMD_WITH_SUBCMDS(tkey, "Tillitis TKey security token operations",
			tkey_help_text,
	U_BOOT_SUBCMD_MKENT(connect, 1, 1, do_tkey_connect),
	U_BOOT_SUBCMD_MKENT(fwmode, 1, 1, do_tkey_fwmode),
	U_BOOT_SUBCMD_MKENT(getkey, 3, 1, do_tkey_getkey),
	U_BOOT_SUBCMD_MKENT(info, 1, 1, do_tkey_info),
	U_BOOT_SUBCMD_MKENT(loadapp, 2, 1, do_tkey_loadapp),
	U_BOOT_SUBCMD_MKENT(signer, 1, 1, do_tkey_signer),
	U_BOOT_SUBCMD_MKENT(wrapkey, 2, 1, do_tkey_wrapkey));
