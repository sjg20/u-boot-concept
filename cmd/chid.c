// SPDX-License-Identifier: GPL-2.0+
/*
 * Command for Computer Hardware Identifiers (Windows CHID)
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <chid.h>
#include <command.h>
#include <env.h>
#include <vsprintf.h>
#include <linux/bitops.h>
#include <u-boot/uuid.h>

static int do_chid_show(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	struct chid_data chid;
	int ret;

	ret = chid_from_smbios(&chid);
	if (ret) {
		printf("Failed to get CHID data from SMBIOS (err=%dE)\n", ret);
		return CMD_RET_FAILURE;
	}

	printf("Manufacturer:      %s\n", chid.manuf);
	printf("Family:            %s\n", chid.family);
	printf("Product Name:      %s\n", chid.product_name);
	printf("Product SKU:       %s\n", chid.product_sku);
	printf("Baseboard Manuf:   %s\n", chid.board_manuf);
	printf("Baseboard Product: %s\n", chid.board_product);
	printf("BIOS Vendor:       %s\n", chid.bios_vendor);
	printf("BIOS Version:      %s\n", chid.bios_version);
	printf("BIOS Major:        %u\n", chid.bios_major);
	printf("BIOS Minor:        %u\n", chid.bios_minor);
	printf("Enclosure Type:    %u\n", chid.enclosure_type);

	return 0;
}

static int do_chid_list(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	char chid_str[UUID_STR_LEN + 1];
	u8 chid_bytes[UUID_LEN];
	struct chid_data chid;
	int variant, ret;

	ret = chid_from_smbios(&chid);
	if (ret) {
		printf("Failed to get CHID data from SMBIOS (err=%d)\n", ret);
		return CMD_RET_FAILURE;
	}

	for (variant = 0; variant < CHID_VARIANT_COUNT; variant++) {
		ret = chid_generate(variant, &chid, chid_bytes);
		if (ret) {
			printf("%s: <generation failed>\n",
			       chid_get_variant_name(variant));
			continue;
		}

		uuid_bin_to_str(chid_bytes, chid_str, UUID_STR_FORMAT_STD);
		printf("%s: %s\n", chid_get_variant_name(variant), chid_str);
	}

	return 0;
}

static int do_chid_detail(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	char chid_str[UUID_STR_LEN + 1];
	u8 chid_bytes[UUID_LEN];
	struct chid_data chid;
	int fields, field;
	int variant, ret;
	bool first;

	if (argc != 2) {
		printf("Usage: chid show <variant>\n");
		return CMD_RET_USAGE;
	}

	variant = simple_strtol(argv[1], NULL, 10);
	if (variant < 0 || variant >= CHID_VARIANT_COUNT) {
		printf("Invalid variant %d (must be 0-%d)\n", variant,
		       CHID_VARIANT_COUNT - 1);
		return CMD_RET_FAILURE;
	}

	ret = chid_from_smbios(&chid);
	if (ret) {
		printf("Failed to get CHID data from SMBIOS (err=%dE)\n", ret);
		return CMD_RET_FAILURE;
	}

	ret = chid_generate(variant, &chid, chid_bytes);
	if (ret) {
		printf("Failed to generate CHID variant %d (err=%dE)\n",
		       variant, ret);
		return CMD_RET_FAILURE;
	}

	uuid_bin_to_str(chid_bytes, chid_str, UUID_STR_FORMAT_STD);
	printf("%s: %s\n", chid_get_variant_name(variant), chid_str);

	/* Show which fields are used */
	printf("Fields: ");
	fields = chid_get_variant_fields(variant);
	first = true;

	for (field = 0; field < CHID_COUNT; field++) {
		if (fields & BIT(field)) {
			if (!first)
				printf(" + ");
			printf("%s", chid_get_field_name(field));
			first = 0;
		}
	}
	printf("\n");

	return 0;
}

static int do_chid_compat(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	const char *compat;
	int ret;

	ret = chid_select(&compat);
	if (ret) {
		printf("No compatible string found (err=%d)\n", ret);
		return CMD_RET_FAILURE;
	}

	printf("%s\n", compat);

	ret = env_set("fdtcompat", compat);
	if (ret) {
		printf("Failed to set fdtcompat environment variable (err=%d)\n", ret);
		return CMD_RET_FAILURE;
	}

	return 0;
}

U_BOOT_LONGHELP(chid,
	"compat - Find compatible string and set fdtcompat env var\n"
	"list - List all CHID variants\n"
	"show - Show CHID data extracted from SMBIOS\n"
	"detail <variant> - Show details for a specific CHID variant (0-14)");

U_BOOT_CMD_WITH_SUBCMDS(chid, "Computer Hardware ID utilities", chid_help_text,
	U_BOOT_SUBCMD_MKENT(compat, 1, 1, do_chid_compat),
	U_BOOT_SUBCMD_MKENT(list, 1, 1, do_chid_list),
	U_BOOT_SUBCMD_MKENT(show, 1, 1, do_chid_show),
	U_BOOT_SUBCMD_MKENT(detail, 2, 1, do_chid_detail));
