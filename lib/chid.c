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
#include <linux/bitops.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * Microsoft CHID variants table
 *
 * Each entry defines which SMBIOS fields are combined to create
 * a specific Hardware ID variant. The variants are ordered from
 * most specific (HardwareID-00) to least specific (HardwareID-14).
 */
static const struct chid_variant chid_variants[CHID_VARIANT_COUNT] = {{
	/* HardwareID-00: Most specific - includes all identifying fields */
	.name = "HardwareID-00",
	.fields = BIT(CHID_MANUF) | BIT(CHID_FAMILY) | BIT(CHID_PRODUCT_NAME) |
		BIT(CHID_PRODUCT_SKU) | BIT(CHID_BIOS_VENDOR) |
		BIT(CHID_BIOS_VERSION) | BIT(CHID_BIOS_MAJOR) |
		BIT(CHID_BIOS_MINOR),
	}, {
	/* HardwareID-01: Without SKU */
	.name = "HardwareID-01",
	.fields = BIT(CHID_MANUF) | BIT(CHID_FAMILY) | BIT(CHID_PRODUCT_NAME) |
		BIT(CHID_BIOS_VENDOR) | BIT(CHID_BIOS_VERSION) |
		BIT(CHID_BIOS_MAJOR) | BIT(CHID_BIOS_MINOR),
	}, {
	/* HardwareID-02: Without family */
	.name = "HardwareID-02",
	.fields = BIT(CHID_MANUF) | BIT(CHID_PRODUCT_NAME) |
		BIT(CHID_BIOS_VENDOR) | BIT(CHID_BIOS_VERSION) |
		BIT(CHID_BIOS_MAJOR) | BIT(CHID_BIOS_MINOR),
	}, {
	/* HardwareID-03: With baseboard info, no BIOS version */
	.name = "HardwareID-03",
	.fields = BIT(CHID_MANUF) | BIT(CHID_FAMILY) | BIT(CHID_PRODUCT_NAME) |
		BIT(CHID_PRODUCT_SKU) | BIT(CHID_BOARD_MANUF) |
		BIT(CHID_BOARD_PRODUCT),
	}, {
	/* HardwareID-04: Basic product identification */
	.name = "HardwareID-04",
	.fields = BIT(CHID_MANUF) | BIT(CHID_FAMILY) | BIT(CHID_PRODUCT_NAME) |
		BIT(CHID_PRODUCT_SKU),
	}, {
	/* HardwareID-05: Without SKU */
	.name = "HardwareID-05",
	.fields = BIT(CHID_MANUF) | BIT(CHID_FAMILY) | BIT(CHID_PRODUCT_NAME),
	}, {
	/* HardwareID-06: SKU with baseboard */
	.name = "HardwareID-06",
	.fields = BIT(CHID_MANUF) | BIT(CHID_PRODUCT_SKU) |
		BIT(CHID_BOARD_MANUF) | BIT(CHID_BOARD_PRODUCT),
	}, {
	/* HardwareID-07: Just manufacturer and SKU */
	.name = "HardwareID-07",
	.fields = BIT(CHID_MANUF) | BIT(CHID_PRODUCT_SKU),
	}, {
	/* HardwareID-08: Product name with baseboard */
	.name = "HardwareID-08",
	.fields = BIT(CHID_MANUF) | BIT(CHID_PRODUCT_NAME) |
		BIT(CHID_BOARD_MANUF) | BIT(CHID_BOARD_PRODUCT),
	}, {
	/* HardwareID-09: Just manufacturer and product name */
	.name = "HardwareID-09",
	.fields = BIT(CHID_MANUF) | BIT(CHID_PRODUCT_NAME),
	}, {
	/* HardwareID-10: Family with baseboard */
	.name = "HardwareID-10",
	.fields = BIT(CHID_MANUF) | BIT(CHID_FAMILY) | BIT(CHID_BOARD_MANUF) |
		BIT(CHID_BOARD_PRODUCT),
	}, {
	/* HardwareID-11: Just manufacturer and family */
	.name = "HardwareID-11",
	.fields = BIT(CHID_MANUF) | BIT(CHID_FAMILY),
	}, {
	/* HardwareID-12: Manufacturer and enclosure type */
	.name = "HardwareID-12",
	.fields = BIT(CHID_MANUF) | BIT(CHID_ENCLOSURE_TYPE),
	}, {
	/* HardwareID-13: Manufacturer with baseboard only */
	.name = "HardwareID-13",
	.fields = BIT(CHID_MANUF) | BIT(CHID_BOARD_MANUF) |
		BIT(CHID_BOARD_PRODUCT),
	}, {
	/* HardwareID-14: Least specific - manufacturer only */
	.name = "HardwareID-14",
	.fields = BIT(CHID_MANUF),
	}
};

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
