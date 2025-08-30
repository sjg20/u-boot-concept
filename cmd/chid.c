// SPDX-License-Identifier: GPL-2.0+
/*
 * Command for Computer Hardware Identifiers (Windows CHID)
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <chid.h>
#include <command.h>
#include <vsprintf.h>

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

	return CMD_RET_SUCCESS;
}

U_BOOT_LONGHELP(chid,
	"show - Show CHID data extracted from SMBIOS");

U_BOOT_CMD_WITH_SUBCMDS(chid, "Computer Hardware ID utilities", chid_help_text,
	U_BOOT_SUBCMD_MKENT(show, 1, 1, do_chid_show));
