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
 * These enum values are used as BIT(x) values that can be ORed together
 * to define which fields are used in each CHID variant.
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

/* Number of standard Microsoft CHID variants */
#define CHID_VARIANT_COUNT	15

/**
 * struct chid_variant - defines which fields are used in each CHID variant
 * @name: Human-readable name for debugging
 * @fields: Bitmask of fields (BIT(CHID_xxx) values ORed together)
 */
struct chid_variant {
	const char *name;
	u32 fields;
};

/**
 * struct chid_data - contains the actual SMBIOS field values
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
 * @chid: Pointer to CHID data structure to fill
 *
 * Return: 0 if OK, -ve error code on failure
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
 * chid_generate_all() - Generate all CHID variants
 * @data: SMBIOS data to use for generation  
 * @chids: Output buffer for all CHIDs (15 x 16 bytes)
 *
 * Return: 0 if OK, -ve error code on failure
 */
int chid_generate_all(const struct chid_data *data, u8 chids[15][16]);

#endif
