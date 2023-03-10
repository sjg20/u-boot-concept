/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2022-2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * Authors:
 *   Abdellatif El Khlifi <abdellatif.elkhlifi@arm.com>
 */

#ifndef __SANDBOX_ARM_FFA_PRV_H
#define __SANDBOX_ARM_FFA_PRV_H

#include <sandbox_arm_ffa.h>
#include "arm_ffa_priv.h"

/* This header is exclusively used by the Sandbox FF-A driver and sandbox tests */

/* FF-A core driver name */
#define FFA_SANDBOX_DRV_NAME "sandbox_arm_ffa"

/* FF-A ABIs internal error codes (as defined by the spec) */

#define FFA_ERR_STAT_NOT_SUPPORTED	-1
#define FFA_ERR_STAT_INVALID_PARAMETERS	-2
#define FFA_ERR_STAT_NO_MEMORY	-3
#define FFA_ERR_STAT_BUSY	-4
#define FFA_ERR_STAT_DENIED	-6

/* Non-secure physical FF-A instance */
#define NS_PHYS_ENDPOINT_ID (0)

#define GET_NS_PHYS_ENDPOINT_ID_MASK		GENMASK(31, 16)
#define GET_NS_PHYS_ENDPOINT_ID(x)		\
			((u16)(FIELD_GET(GET_NS_PHYS_ENDPOINT_ID_MASK, (x))))

/* Helper macro for reading the destination partition ID */
#define GET_DST_SP_ID_MASK		GENMASK(15, 0)
#define GET_DST_SP_ID(x)		\
			((u16)(FIELD_GET(GET_DST_SP_ID_MASK, (x))))

/* Helper macro for setting the source partition ID */
#define PREP_SRC_SP_ID_MASK		GENMASK(31, 16)
#define PREP_SRC_SP_ID(x)		\
			(FIELD_PREP(PREP_SRC_SP_ID_MASK, (x)))

/* Helper macro for setting the destination endpoint ID */
#define PREP_NS_PHYS_ENDPOINT_ID_MASK		GENMASK(15, 0)
#define PREP_NS_PHYS_ENDPOINT_ID(x)		\
			(FIELD_PREP(PREP_NS_PHYS_ENDPOINT_ID_MASK, (x)))

/*  RX/TX buffers minimum size */
#define RXTX_BUFFERS_MIN_SIZE (RXTX_4K)
#define RXTX_BUFFERS_MIN_PAGES (1)

/* MBZ registers info */

/* x1-x7 MBZ */
#define FFA_X1X7_MBZ_CNT (7)
#define FFA_X1X7_MBZ_REG_START (&res->a1)

/* x4-x7 MBZ */
#define FFA_X4X7_MBZ_CNT (4)
#define FFA_X4X7_MBZ_REG_START (&res->a4)

/* x3-x7 MBZ */
#define FFA_X3X7_MBZ_CNT (5)
#define FFA_X3_MBZ_REG_START (&res->a3)

/* number of secure partitions emulated by the FF-A sandbox driver */
#define SANDBOX_PARTITIONS_CNT (4)

/* Binary data of services UUIDs emulated by the FF-A sandbox driver */

/* service 1  UUID binary data (little-endian format) */
#define SANDBOX_SERVICE1_UUID_A1	0xed32d533
#define SANDBOX_SERVICE1_UUID_A2	0x99e64209
#define SANDBOX_SERVICE1_UUID_A3	0x9cc02d72
#define SANDBOX_SERVICE1_UUID_A4	0xcdd998a7

/* service 2  UUID binary data (little-endian format) */
#define SANDBOX_SERVICE2_UUID_A1	0xed32d544
#define SANDBOX_SERVICE2_UUID_A2	0x99e64209
#define SANDBOX_SERVICE2_UUID_A3	0x9cc02d72
#define SANDBOX_SERVICE2_UUID_A4	0xcdd998a7

/**
 * struct ffa_rxtxpair_info - structure hosting the RX/TX buffers flags
 * @rxbuf_owned:	RX buffer ownership flag (the owner is non secure world: the consumer)
 * @rxbuf_mapped:	RX buffer mapping flag
 * @txbuf_owned	TX buffer ownership flag
 * @txbuf_mapped:	TX buffer mapping flag
 * @rxtx_buf_size:	RX/TX buffers size as set by the FF-A core driver
 *
 * Data structure hosting the ownership/mapping flags of the RX/TX buffers
 * When a buffer is owned/mapped its corresponding flag is set to 1 otherwise 0.
 */
struct ffa_rxtxpair_info {
	u8 rxbuf_owned;
	u8 rxbuf_mapped;
	u8 txbuf_owned;
	u8 txbuf_mapped;
	u32 rxtx_buf_size;
};

/**
 * struct sandbox_ffa_priv - the driver private data structure
 *
 * @dev:	The arm_ffa device under u-boot driver model
 * @fwk_version:	FF-A framework version
 * @id:	u-boot endpoint ID
 * @partitions:	The partitions descriptors structure
 * @pair:	The RX/TX buffers pair
 * @pair_info:	The RX/TX buffers pair flags and size
 * @conduit:	The selected conduit
 *
 * The driver data structure hosting all the emulated secure world data.
 */
struct sandbox_ffa_priv {
	struct udevice *dev;
	u32 fwk_version;
	u16 id;
	struct ffa_partitions partitions;
	struct ffa_rxtxpair pair;
	struct ffa_rxtxpair_info pair_info;
};

#define SANDBOX_SMC_FFA_ABI(ffabi) static int sandbox_##ffabi(struct udevice *dev, \
							      ffa_value_t *pargs, ffa_value_t *res)

#endif
