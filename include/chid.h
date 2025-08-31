/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Computer Hardware Identifiers (Windows CHID)
 *
 * See: https://github.com/fwupd/fwupd/blob/main/docs/hwids.md
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#ifndef __chid_h
#define __chid_h

#include <linux/types.h>

/**
 * enum chid_field_t - fields we pick up from SMBIOS tables
 *
 * Used as BIT(x) values that can be ORed together to define which fields are
 * used in each CHID variant.
 *
 * The table and field name is shown here (see smbios.h). All are strings
 * except those noted as int.
 *
 * @CHID_MANUF: SMBIOS Type 1 (System Information): manufacturer
 * @CHID_FAMILY: SMBIOS Type 1 (System Information): family
 * @CHID_PRODUCT_NAME: SMBIOS Type 1 (System Information): product_name
 * @CHID_PRODUCT_SKU: SMBIOS Type 1 (System Information): sku_number
 * @CHID_BOARD_MANUF: SMBIOS Type 2 (Baseboard Information): manufacturer
 * @CHID_BOARD_PRODUCT: SMBIOS Type 2 (Baseboard Information): product_name
 * @CHID_BIOS_VENDOR: SMBIOS Type 0 (BIOS Information): vendor
 * @CHID_BIOS_VERSION: SMBIOS Type 0 (BIOS Information): bios_ver
 * @CHID_BIOS_MAJOR: SMBIOS Type 0 (BIOS Information): bios_major_release (int)
 * @CHID_BIOS_MINOR: SMBIOS Type 0 (BIOS Information): bios_minor_release (int)
 * @CHID_ENCLOSURE_TYPE: SMBIOS Type 3 (System Enclosure): chassis_type (int)
 * @CHID_COUNT: Number of CHID fields
 */
enum chid_field_t {
	CHID_MANUF,
	CHID_FAMILY,
	CHID_PRODUCT_NAME,
	CHID_PRODUCT_SKU,
	CHID_BOARD_MANUF,
	CHID_BOARD_PRODUCT,
	CHID_BIOS_VENDOR,
	CHID_BIOS_VERSION,
	CHID_BIOS_MAJOR,
	CHID_BIOS_MINOR,
	CHID_ENCLOSURE_TYPE,

	CHID_COUNT,
};

/*
 * enum chid_variant_id - Microsoft CHID hardware ID variants
 *
 * This covers HardwareID-00 through HardwareID-14
 */
enum chid_variant_id {
	CHID_00,	/* Most specific */
	CHID_01,
	CHID_02,
	CHID_03,
	CHID_04,
	CHID_05,
	CHID_06,
	CHID_07,
	CHID_08,
	CHID_09,
	CHID_10,
	CHID_11,
	CHID_12,
	CHID_13,
	CHID_14,	/* Least specific */
	
	CHID_VARIANT_COUNT
};

/**
 * struct chid_variant - defines which fields are used in each CHID variant
 *
 * @name: Human-readable name for debugging
 * @fields: Bitmask of fields (BIT(CHID_xxx) values ORed together)
 */
struct chid_variant {
	const char *name;
	u32 fields;
};

/**
 * struct chid_data - contains SMBIOS field values to use in calculating CHID
 *
 * There is one field here for each item in enum chid_field_t
 *
 * @manuf: System manufacturer string
 * @family: Product family string
 * @product_name: Product name string
 * @product_sku: Product SKU string
 * @board_manuf: Baseboard manufacturer string
 * @board_product: Baseboard product string
 * @bios_vendor: BIOS vendor string
 * @bios_version: BIOS version string
 * @bios_major: BIOS major version number
 * @bios_minor: BIOS minor version number
 * @enclosure_type: System enclosure type
 */
struct chid_data {
	const char *manuf;
	const char *family;
	const char *product_name;
	const char *product_sku;
	const char *board_manuf;
	const char *board_product;
	const char *bios_vendor;
	const char *bios_version;
	u8 bios_major;
	u8 bios_minor;
	u8 enclosure_type;
};

/**
 * chid_from_smbios() - Extract CHID data from SMBIOS tables
 *
 * @chid: Pointer to CHID data structure to fill
 *
 * Return: 0 if OK, -ENOENT if a required table is missing (SMBIOS types 0-1),
 * other -ve error code if the SMBIOS tables cannot be found (see
 * smbios_locate())
 */
int chid_from_smbios(struct chid_data *chid);

/**
 * chid_generate() - Generate a specific CHID variant
 *
 * @variant: Which CHID variant to generate (0-14)
 * @data: SMBIOS data to use for generation
 * @chid: Output buffer for the generated CHID (16 bytes)
 *
 * Return: 0 if OK, -ve error code on failure
 */
int chid_generate(int variant, const struct chid_data *data, u8 chid[16]);

/**
 * chid_get_field_name() - Get display name of a specific CHID field
 *
 * @field: Which CHID field
 *
 * Return: String containing the field name
 */
const char *chid_get_field_name(enum chid_field_t field);

/**
 * chid_get_variant_fields() - Get the fields mask for a CHID variant
 *
 * @variant: Which CHID variant (0-14)
 *
 * Return: Bitmask of fields used by this variant
 */
u32 chid_get_variant_fields(int variant);

#endif
