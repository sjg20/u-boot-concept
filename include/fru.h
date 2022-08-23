/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2019 Xilinx, Inc.
 * Siva Durga Prasad Paladugu <siva.durga.paladugu@xilinx.com>
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __FRU_H
#define __FRU_H

#include <linux/list.h>

/**
 * struct fru_common_hdr - FRU common header
 *
 * @version:		Common header format version
 * @off_internal:	Internal use area starting offset
 * @off_chassis:	Chassis info area starting offset
 * @off_board:		Board area starting offset
 * @off_product:	Product info area starting offset
 * @off_multirec:	MultiRecord area starting offset
 * @pad:		PAD, write as 00h
 * @crc:		Common header checksum (zero checksum)
 *
 * Offsets are all in multiples of 8 bytes). 00h indicates that the area is not
 * present.
 */
struct fru_common_hdr {
	u8 version;
	u8 off_internal;
	u8 off_chassis;
	u8 off_board;
	u8 off_product;
	u8 off_multirec;
	u8 pad;
	u8 crc;
} __packed;

#define FRU_INFO_FIELD_LEN_MAX		32
#define FRU_MULTIREC_DATA_LEN_MAX	255

/**
 * struct fru_board_info_header - Board info area header
 *
 * @ver:	Board area format version
 * @len:	Board area length (in multiples of 8 bytes)
 * @lang_code:	Language code
 * @time:	Mfg. date / time
 *		Number of minutes from 0:00 hrs 1/1/96.
 *		LSbyte first (little endian)
 *		00_00_00h = unspecified
 */
struct fru_board_info_header {
	u8 ver;
	u8 len;
	u8 lang_code;
	u8 time[3];
} __packed;

/**
 * struct fru_product_info_header - Product info area header
 *
 * @ver:	Product area format version
 * @len:	Product area length (in multiples of 8 bytes)
 * @lang_code:	Language code
 */
struct fru_product_info_header {
	u8 ver;
	u8 len;
	u8 lang_code;
} __packed;

/**
 * struct fru_common_info_member - FRU common info member
 *
 * @type_len:	type/length byte
 * @name:	Member information bytes
 */
struct fru_common_info_member {
	u8 type_len;
	u8 *name;
} __packed;

/**
 * struct fru_custom_info - Custom info field
 *
 * @type_len:	Type/length byte
 * @data:	Custom information bytes
 */
struct fru_custom_info {
	u8 type_len;
	u8 data[FRU_INFO_FIELD_LEN_MAX];
} __packed;

/**
 * struct fru_custom_field_node - Linked list head for Custom info fields
 *
 * @list:	Linked list head
 * @info:	Custom info field of the node
 */
struct fru_custom_field_node {
	struct list_head list;
	struct fru_custom_info info;
};

/**
 * struct fru_board_data - Board info area
 *
 * @ver:			Board area format version
 * @len:			Board area length (in multiples of 8 bytes)
 * @lang_code:			Language code
 * @time:			Mfg. date / time
 * @manufacturer_type_len:	Type/length byte
 * @manufacturer_name:		Board manufacturer name
 * @product_name_type_len:	Type/length byte
 * @product_name:		Board product name
 * @serial_number_type_len:	Type/length byte
 * @serial_number:		Board serial number
 * @part_number_type_len:	Type/length byte
 * @part_number:		Board part number
 * @file_id_type_len:		Type/length byte
 * @file_id:			FRU file ID
 * @custom_fields:		Linked list head for Custom info fields
 */
struct fru_board_data {
	u8 ver;
	u8 len;
	u8 lang_code;
	u8 time[3];
	u8 manufacturer_type_len;
	u8 manufacturer_name[FRU_INFO_FIELD_LEN_MAX];
	u8 product_name_type_len;
	u8 product_name[FRU_INFO_FIELD_LEN_MAX];
	u8 serial_number_type_len;
	u8 serial_number[FRU_INFO_FIELD_LEN_MAX];
	u8 part_number_type_len;
	u8 part_number[FRU_INFO_FIELD_LEN_MAX];
	u8 file_id_type_len;
	u8 file_id[FRU_INFO_FIELD_LEN_MAX];
	struct list_head custom_fields;
};

/**
 * struct fru_product_data - Product info area
 *
 * @ver:			Product area format version
 * @len:			Product area length (in multiples of 8 bytes)
 * @lang_code:			Language code
 * @manufacturer_type_len:	Type/length byte
 * @manufacturer_name:		Product manufacturer name
 * @product_name_type_len:	Type/length byte
 * @product_name:		Product name
 * @part_number_type_len:	Type/length byte
 * @part_number:		Product part number
 * @version_number_type_len:	Type/length byte
 * @version_number:		Product version number
 * @serial_number_type_len:	Type/length byte
 * @serial_number:		Product serial number
 * @asset_number_type_len:	Type/length byte
 * @asset_number:		Product asset number
 * @file_id_type_len:		Type/length byte
 * @file_id:			FRU file ID
 * @custom_fields:		Linked list head for Custom info fields
 */
struct fru_product_data {
	u8 ver;
	u8 len;
	u8 lang_code;
	u8 manufacturer_type_len;
	u8 manufacturer_name[FRU_INFO_FIELD_LEN_MAX];
	u8 product_name_type_len;
	u8 product_name[FRU_INFO_FIELD_LEN_MAX];
	u8 part_number_type_len;
	u8 part_number[FRU_INFO_FIELD_LEN_MAX];
	u8 version_number_type_len;
	u8 version_number[FRU_INFO_FIELD_LEN_MAX];
	u8 serial_number_type_len;
	u8 serial_number[FRU_INFO_FIELD_LEN_MAX];
	u8 asset_number_type_len;
	u8 asset_number[FRU_INFO_FIELD_LEN_MAX];
	u8 file_id_type_len;
	u8 file_id[FRU_INFO_FIELD_LEN_MAX];
	struct list_head custom_fields;
};

/**
 * struct fru_multirec_hdr - MultiRecord area header
 *
 * @rec_type:	Product area format version
 * @type:	Product area length (in multiples of 8 bytes)
 * @len:	Language code
 * @csum:	Type/length byte
 * @hdr_csum:	Product manufacturer name
 */
struct fru_multirec_hdr {
	u8 rec_type;
	u8 type;
	u8 len;
	u8 csum;
	u8 hdr_csum;
} __packed;

/**
 * struct fru_multirec_info - MultiRecord info field
 *
 * @hdr:	MultiRecord area header
 * @data:	MultiRecord information bytes
 */
struct fru_multirec_info {
	struct fru_multirec_hdr hdr;
	u8 data[FRU_MULTIREC_DATA_LEN_MAX];
} __packed;

/**
 * struct fru_multirec_node - Linked list head for MultiRecords
 *
 * @list:	Linked list head
 * @info:	MultiRecord info field of the node
 */
struct fru_multirec_node {
	struct list_head list;
	struct fru_multirec_info info;
};

/**
 * struct fru_table - FRU table storage
 *
 * @hdr:	FRU common header
 * @brd:	Board info
 * @prd:	Product info
 * @multi_recs:	MultiRecords
 * @captured:	TRUE when this table is captured and parsed
 */
struct fru_table {
	struct fru_common_hdr hdr;
	struct fru_board_data brd;
	struct fru_product_data prd;
	struct list_head multi_recs;
	bool captured;
};

#define FRU_TYPELEN_CODE_MASK		0xC0
#define FRU_TYPELEN_LEN_MASK		0x3F
#define FRU_COMMON_HDR_VER_MASK		0xF
#define FRU_COMMON_HDR_LEN_MULTIPLIER	8
#define FRU_LANG_CODE_ENGLISH		0
#define FRU_LANG_CODE_ENGLISH_1		25
#define FRU_TYPELEN_EOF			0xC1
#define FRU_LAST_REC			BIT(7)
#define FRU_MULTIREC_TYPE_OEM_START	0xC0
#define FRU_MULTIREC_TYPE_OEM_END	0xFF

/* This should be minimum of fields */
#define FRU_BOARD_AREA_TOTAL_FIELDS	5
#define FRU_PRODUCT_AREA_TOTAL_FIELDS	7
#define FRU_TYPELEN_TYPE_SHIFT		6
#define FRU_TYPELEN_TYPE_BINARY		0
#define FRU_TYPELEN_TYPE_ASCII8		3

/**
 * fru_display() - display captured FRU information
 *
 * @verbose:	Enable (1) verbose output or not (0)
 *
 * Returns 0 on success or a negative error code.
 */
int fru_display(int verbose);

/**
 * fru_display() - parse and capture FRU configuration table
 *
 * @addr:	Address where the FRU configuration table is stored
 *
 * Returns 0 on success or a negative error code.
 */
int fru_capture(const void *addr);

/**
 * fru_board_generate() - generate FRU which has board info area
 *
 * @addr:	Address to store generated FRU configuration table at
 * @argc:	Length of @argv
 * @argv:	Vector of arguments. See doc/usage/fru.rst for more details
 *
 * Returns 0 on success or a negative error code.
 */
int fru_board_generate(const void *addr, int argc, char *const argv[]);

/**
 * fru_product_generate() - generate FRU which has product info area
 *
 * @addr:	Address to store generated FRU configuration table at
 * @argc:	Length of @argv
 * @argv:	Vector of arguments. See doc/usage/fru.rst for more details
 *
 * Returns 0 on success or a negative error code.
 */
int fru_product_generate(const void *addr, int argc, char *const argv[]);

/**
 * fru_checksum() - calculate checksum of FRU info
 *
 * @addr:	Address of the FRU info data source
 * @len:	Length of the FRU info data
 *
 * Returns a calculated checksum.
 */
u8 fru_checksum(u8 *addr, u8 len);

/**
 * fru_check_type_len() - check and parse type/len byte
 *
 * @type_len:	Type/len byte
 * @language:	Language code byte
 * @type:	Pointer to a variable to store parsed type
 *
 * Returns length of the type, -EINVAL on the FRU_TYPELEN_EOF type
 */
int fru_check_type_len(u8 type_len, u8 language, u8 *type);

/**
 * fru_get_fru_data() - get pointer to captured FRU info table
 *
 * Returns pointer to captured FRU table
 */
const struct fru_table *fru_get_fru_data(void);

#endif /* FRU_H */
