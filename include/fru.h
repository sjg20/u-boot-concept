/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2019 Xilinx, Inc.
 * Siva Durga Prasad Paladugu <siva.durga.paladugu@xilinx.com>
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __FRU_H
#define __FRU_H

#include <linux/list.h>

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

struct fru_board_info_header {
	u8 ver;
	u8 len;
	u8 lang_code;
	u8 time[3];
} __packed;

struct fru_board_info_member {
	u8 type_len;
	u8 *name;
} __packed;

struct fru_custom_info {
	u8 type_len;
	u8 data[FRU_INFO_FIELD_LEN_MAX];
} __packed;

struct fru_custom_field_node {
	struct list_head list;
	struct fru_custom_info info;
};

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

struct fru_multirec_hdr {
	u8 rec_type;
	u8 type;
	u8 len;
	u8 csum;
	u8 hdr_csum;
} __packed;

struct fru_multirec_info {
	struct fru_multirec_hdr hdr;
	u8 data[FRU_MULTIREC_DATA_LEN_MAX];
} __packed;

struct fru_multirec_node {
	struct list_head list;
	struct fru_multirec_info info;
};

struct fru_table {
	struct fru_common_hdr hdr;
	struct fru_board_data brd;
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
#define FRU_TYPELEN_TYPE_SHIFT		6
#define FRU_TYPELEN_TYPE_BINARY		0
#define FRU_TYPELEN_TYPE_ASCII8		3

int fru_display(int verbose);
int fru_capture(unsigned long addr);
int fru_generate(unsigned long addr, int argc, char *const argv[]);
u8 fru_checksum(u8 *addr, u8 len);
int fru_check_type_len(u8 type_len, u8 language, u8 *type);
const struct fru_table *fru_get_fru_data(void);

#endif /* FRU_H */
