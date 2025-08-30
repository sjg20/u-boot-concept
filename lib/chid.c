// SPDX-License-Identifier: GPL-2.0+
/*
 * Computer Hardware Identifiers (Windows CHID) support
 *
 * This implements the Microsoft Computer Hardware ID specification
 * used by Windows Update and fwupd for hardware identification.
 *
 * See: https://github.com/fwupd/fwupd/blob/main/docs/hwids.md
 * See: https://docs.microsoft.com/en-us/windows-hardware/drivers/install/specifying-hardware-ids-for-a-computer
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <chid.h>
#include <smbios.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

int chid_from_smbios(struct chid_data *chid)
{
	const struct smbios_type0 *bios;
	const struct smbios_type1 *sys;
	const struct smbios_type2 *board;
	const struct smbios_type3 *encl;
	struct smbios_info info;
	int ret;

	/* Clear the structure first */
	memset(chid, '\0', sizeof(*chid));

	ret = smbios_locate(gd_smbios_start(), &info);
	if (ret)
		return ret;

	/* Extract BIOS Information (Type 0) */
	bios = smbios_get_header(&info, SMBIOS_BIOS_INFORMATION);
	if (bios) {
		chid->bios_vendor = smbios_string(&bios->hdr, bios->vendor);
		chid->bios_version = smbios_string(&bios->hdr, bios->bios_ver);
		chid->bios_major = bios->bios_major_release;
		chid->bios_minor = bios->bios_minor_release;
	}

	/* Extract System Information (Type 1) */
	sys = smbios_get_header(&info, SMBIOS_SYSTEM_INFORMATION);
	if (sys) {
		chid->manuf = smbios_string(&sys->hdr, sys->manufacturer);
		chid->product_name = smbios_string(&sys->hdr,
						   sys->product_name);
		chid->family = smbios_string(&sys->hdr, sys->family);
		chid->product_sku = smbios_string(&sys->hdr, sys->sku_number);
	}

	/* Extract Baseboard Information (Type 2) */
	board = smbios_get_header(&info, SMBIOS_BOARD_INFORMATION);
	if (board) {
		chid->board_manuf = smbios_string(&board->hdr,
						  board->manufacturer);
		chid->board_product = smbios_string(&board->hdr,
						    board->product_name);
	}

	/* Extract System Enclosure Information (Type 3) */
	encl = smbios_get_header(&info, SMBIOS_SYSTEM_ENCLOSURE);
	if (encl)
		chid->enclosure_type = encl->chassis_type;

	return 0;
}
