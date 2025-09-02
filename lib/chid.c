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
 *
 * Credit: Richard Hughes
 * https://blogs.gnome.org/hughsie/2017/04/25/reverse-engineering-computerhardwareids-exe-with-winedbg/
 */

#define LOG_CATEGORY	LOGC_ACPI

#include <chid.h>
#include <errno.h>
#include <log.h>
#include <malloc.h>
#include <smbios.h>
#include <asm/global_data.h>
#include <dm/device.h>
#include <dm/ofnode.h>
#include <linux/bitops.h>
#include <linux/utf.h>
#include <linux/kernel.h>
#include <u-boot/uuid.h>

DECLARE_GLOBAL_DATA_PTR;

/**
 * struct dt_chid_node - contains CHID retrievd from the devicetree
 *
 * @node: devicetree node containing CHID info
 * @compatible: compatible string for this node
 * @variant: CHID variant number (0-14)
 * @fields: bitmask of fields used in CHID generation
 * @chid: 16-byte CHID (UUID)
 */
struct dt_chid_node {
	ofnode node;
	const char *compatible;
	int variant;
	u32 fields;
	u8 chid[UUID_LEN];
};

/* field names for display purposes */
static const char *fields[CHID_COUNT] = {
	[CHID_MANUF] = "Manufacturer",
	[CHID_FAMILY] = "Family",
	[CHID_PRODUCT_NAME] = "ProductName",
	[CHID_PRODUCT_SKU] = "ProductSku",
	[CHID_BOARD_MANUF] = "BaseboardManufacturer",
	[CHID_BOARD_PRODUCT] = "BaseboardProduct",
	[CHID_BIOS_VENDOR] = "BiosVendor",
	[CHID_BIOS_VERSION] = "BiosVersion",
	[CHID_BIOS_MAJOR] = "BiosMajorRelease",
	[CHID_BIOS_MINOR] = "BiosMinorRelease",
	[CHID_ENCLOSURE_TYPE] = "EnclosureKind",
};

/*
 * Microsoft CHID variants table
 *
 * Each entry defines which SMBIOS fields are combined to create
 * a specific Hardware ID variant. The variants are ordered from
 * most specific (HardwareID-00) to least specific (HardwareID-14).
 */
static const struct chid_variant variants[CHID_VARIANT_COUNT] = {{
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
	if (!bios)
		return log_msg_ret("cf0", -ENOENT);

	chid->bios_vendor = smbios_string(&bios->hdr, bios->vendor);
	chid->bios_version = smbios_string(&bios->hdr, bios->bios_ver);
	chid->bios_major = bios->bios_major_release;
	chid->bios_minor = bios->bios_minor_release;

	/* Extract System Information (Type 1) */
	sys = smbios_get_header(&info, SMBIOS_SYSTEM_INFORMATION);
	if (!sys)
		return log_msg_ret("cf1", -ENOENT);
	chid->manuf = smbios_string(&sys->hdr, sys->manufacturer);
	chid->product_name = smbios_string(&sys->hdr, sys->product_name);
	chid->family = smbios_string(&sys->hdr, sys->family);
	chid->product_sku = smbios_string(&sys->hdr, sys->sku_number);

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

/**
 * add_item() - Add a string to a buffer if the field is enabled
 *
 * Adds a string and then an '&', but only if the field is enabled in the mask
 *
 * @ptr: Current position in the buffer
 * @end: Pointer to the end of the buffer (one past the last byte)
 * @fields: Bitmask of enabled fields
 * @field: Which field this is (CHID_xxx)
 * @str: String to add, or NULL if none, in which case nothing is added
 * Return: Pointer updated to after the written string and '&' (non-terminated),
 * or @end if out of space
 */
static char *add_item(char *ptr, char *end, u32 fields, enum chid_field_t field,
		      const char *str)
{
	char trimmed[256];
	int len;

	if (!(fields & BIT(field)) || !str)
		return ptr;

	/* Copy string to temporary buffer and trim spaces */
	strlcpy(trimmed, str, sizeof(trimmed));
	str = strim(trimmed);
	if (!*str)
		return ptr;

	len = strlen(str);
	if (end - ptr <= len + 1)
		return end;
	memcpy(ptr, str, len);
	ptr += len;
	*ptr++ = '&';

	return ptr;
}

int chid_generate(int variant, const struct chid_data *data, u8 chid[16])
{
	const struct chid_variant *var;
	struct uuid namespace = {
		.time_low = cpu_to_be32(0x70ffd812),
		.time_mid = cpu_to_be16(0x4c7f),
		.time_hi_and_version = cpu_to_be16(0x4c7d),
	};
	__le16 utf16_data[1024];
	char *ptr, *end;
	int utf16_chars;
	char str[512];
	u32 fields;

	/* Validate input parameters */
	if (variant < 0 || variant >= CHID_VARIANT_COUNT || !data || !chid)
		return -EINVAL;

	var = &variants[variant];
	fields = var->fields;
	ptr = str;
	end = str + sizeof(str) - 1;

	/* build the input string based on the variant's field mask */
	ptr = add_item(ptr, end, fields, CHID_MANUF, data->manuf);
	ptr = add_item(ptr, end, fields, CHID_FAMILY, data->family);
	ptr = add_item(ptr, end, fields, CHID_PRODUCT_NAME, data->product_name);
	ptr = add_item(ptr, end, fields, CHID_PRODUCT_SKU, data->product_sku);
	ptr = add_item(ptr, end, fields, CHID_BOARD_MANUF, data->board_manuf);
	ptr = add_item(ptr, end, fields, CHID_BOARD_PRODUCT,
		       data->board_product);
	ptr = add_item(ptr, end, fields, CHID_BIOS_VENDOR, data->bios_vendor);
	ptr = add_item(ptr, end, fields, CHID_BIOS_VERSION, data->bios_version);
	ptr = add_item(ptr, end, fields, CHID_BIOS_MAJOR,
		       simple_itoa(data->bios_major));
	ptr = add_item(ptr, end, fields, CHID_BIOS_MINOR,
		       simple_itoa(data->bios_minor));
	ptr = add_item(ptr, end, fields, CHID_ENCLOSURE_TYPE,
		       simple_itoa(data->enclosure_type));

	/* Check if we ran out of buffer space */
	if (ptr == end)
		return log_msg_ret("cgs", -ENOSPC);

	/* If no fields were added, we can't generate a CHID */
	if (ptr == str)
		return log_msg_ret("cgn", -ENODATA);

	/* remove the trailing '&' and nul-terminate the string */
	ptr--;
	*ptr = '\0';

	/*
	 * convert to UTF-16LE and generate v5 UUID using Microsoft's namespace
	 * This matches Microsoft's ComputerHardwareIds.exe implementation
	 */
	utf16_chars = utf8_to_utf16le(str, utf16_data, ARRAY_SIZE(utf16_data));
	if (utf16_chars < 0)
		return log_msg_ret("cgu", -ENOMEM);

	gen_v5_guid_be(&namespace, (struct efi_guid *)chid, utf16_data,
		       utf16_chars * 2, NULL);

	return 0;
}

const char *chid_get_field_name(enum chid_field_t field)
{
	if (field >= CHID_COUNT)
		return "Unknown";

	return fields[field];
}

u32 chid_get_variant_fields(int variant)
{
	if (variant < 0 || variant >= CHID_VARIANT_COUNT)
		return 0;

	return variants[variant].fields;
}

const char *chid_get_variant_name(int variant)
{
	if (variant < 0 || variant >= CHID_VARIANT_COUNT)
		return "Invalid";

	return variants[variant].name;
}

bool chid_variant_allowed(enum chid_variant_id variant)
{
	/* Check for invalid variant */
	if (variant < 0 || variant >= CHID_VARIANT_COUNT)
		return false;

	/* Check for prohibited variants */
	switch (variant) {
	case CHID_11:	/* Manufacturer + Family */
	case CHID_12:	/* Manufacturer + EnclosureKind */
	/* Manufacturer + BaseboardManufacturer + BaseboardProduct */
	case CHID_13:
	case CHID_14:	/* Manufacturer only */
		return false;
	default:
		return true;
	}
}

/**
 * chid_extract() - Extract CHID info from hardware-id node
 *
 * @hw_id_node: devicetree hardware-id node to examine
 * @device_node: parent device node (for compatible string)
 * @dt_chid: structure to fill with extracted data
 *
 * Return: 0 if OK, -ve error code on failure
 */
static int chid_extract(ofnode hw_id_node, ofnode device_node,
			struct dt_chid_node *dt_chid)
{
	const char *compatible;
	const u32 *chid_data;
	int len;

	/* Get the compatible string from the parent device node */
	compatible = ofnode_read_string(device_node, "compatible");
	if (!compatible)
		return -ENOENT;

	/* Get CHID variant and fields from hardware-id node */
	dt_chid->variant = ofnode_read_u32_default(hw_id_node, "variant", -1);
	dt_chid->fields = ofnode_read_u32_default(hw_id_node, "fields", 0);

	/* Get the CHID binary data from hardware-id node */
	chid_data = ofnode_read_prop(hw_id_node, "chid", &len);
	if (!chid_data || len != UUID_LEN)
		return -EINVAL;

	/* Fill the structure */
	dt_chid->node = hw_id_node;
	dt_chid->compatible = compatible;

	/* Copy CHID data - handle both byte array and u32 array formats */
	memcpy(dt_chid->chid, chid_data, UUID_LEN);

	return 0;
}

/**
 * check_id() - Check if hardware-id node matches CHID data
 *
 * @hw_id_node: hardware-id node to check
 * @device_node: parent device node (for compatible string)
 * @chid_data: CHID data to match against
 *
 * Return: true if this hardware-id node matches the CHID data, false otherwise
 */
static bool check_id(ofnode hw_id_node, ofnode device_node,
		     const struct chid_data *chid_data)
{
	u8 generated_chid[UUID_LEN];
	struct dt_chid_node info;
	int ret;

	/* Extract CHID info from this hardware-id node */
	ret = chid_extract(hw_id_node, device_node, &info);
	if (ret)
		return false;

	/* Skip prohibited variants */
	if (!chid_variant_allowed(info.variant)) {
		log_debug("chid: skipping prohibited variant %d (%s)\n",
			  info.variant, chid_get_variant_name(info.variant));
		return false;
	}

	/* Generate CHID for this variant and compare */
	ret = chid_generate(info.variant, chid_data, generated_chid);
	if (!ret) {
		/* Check for exact CHID match */
		if (!memcmp(info.chid, generated_chid, UUID_LEN)) {
			log_debug("chid: matched compatible '%s' (variant=%d)\n",
				  info.compatible, info.variant);
			return true;
		}
		log_debug("chid: node %s: variant=%d CHID mismatch\n",
			  info.compatible, info.variant);
	} else {
		log_debug("chid: node %s: variant=%d generate failed: %d\n",
			  info.compatible, info.variant, ret);
	}

	return false;
}

/**
 * chid_find_node() - Find a matching CHID device node in devicetree
 *
 * @chid_data: CHID data to match against
 *
 * Searches the devicetree for a device node under /chid that has
 * a hardware-id child node with a CHID that matches the generated CHID.
 *
 * Return: ofnode of matching device, or ofnode_null() if no match
 */
static ofnode chid_find_node(const struct chid_data *chid_data)
{
	ofnode chid_root, node, hw_id_node;

	/* Find the /chid node */
	chid_root = ofnode_path("/chid");
	if (!ofnode_valid(chid_root))
		return ofnode_null();

	/* Iterate through device nodes (test-device-1, test-device-2, etc.) */
	ofnode_for_each_subnode(node, chid_root) {
		/* Iterate through hardware-id child nodes */
		ofnode_for_each_subnode(hw_id_node, node) {
			if (check_id(hw_id_node, node, chid_data))
				return node;
		}
	}

	return ofnode_null();
}

int chid_select_data(const struct chid_data *chid_data, const char **compatp)
{
	const char *compat;
	ofnode node;

	if (!chid_data || !compatp) {
		log_debug("chid: invalid parameters\n");
		return -EINVAL;
	}

	/* Find matching device node */
	node = chid_find_node(chid_data);
	if (!ofnode_valid(node)) {
		log_debug("chid: no matching CHID found\n");
		return -ENOENT;
	}

	/* Get compatible string from the matched device node */
	compat = ofnode_read_string(node, "compatible");
	if (!compat) {
		log_debug("chid: no compatible string found in matched node\n");
		return -ENOENT;
	}

	*compatp = compat;

	return 0;
}

int chid_select(const char **compatp)
{
	struct chid_data smbios_data;
	const char *compat;
	int ret;

	/* Extract SMBIOS data from current system */
	ret = chid_from_smbios(&smbios_data);
	if (ret) {
		debug("chid: failed to extract SMBIOS data: %d\n", ret);
		return log_msg_ret("cis", ret);
	}

	ret = chid_select_data(&smbios_data, &compat);
	if (ret)
		return log_msg_ret("csd", ret);
	*compatp = compat;

	return 0;
}
