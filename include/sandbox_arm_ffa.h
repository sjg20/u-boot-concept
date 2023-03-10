/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2022-2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * Authors:
 *   Abdellatif El Khlifi <abdellatif.elkhlifi@arm.com>
 */

#ifndef __SANDBOX_ARM_FFA_H
#define __SANDBOX_ARM_FFA_H

#include <arm_ffa.h>

/*
 * This header provides the sandbox driver declarations
 * needed by FF-A driver, armffa command and sandbox tests
 */

/* Providing Arm SMCCC declarations to sandbox */

#define ARM_SMCCC_FAST_CALL		1UL
#define ARM_SMCCC_OWNER_STANDARD	4
#define ARM_SMCCC_SMC_32		0
#define ARM_SMCCC_SMC_64		1
#define ARM_SMCCC_TYPE_SHIFT		31
#define ARM_SMCCC_CALL_CONV_SHIFT	30
#define ARM_SMCCC_OWNER_MASK		0x3f
#define ARM_SMCCC_OWNER_SHIFT		24
#define ARM_SMCCC_FUNC_MASK		0xffff

#define ARM_SMCCC_CALL_VAL(type, calling_convention, owner, func_num) \
	(((type) << ARM_SMCCC_TYPE_SHIFT) | \
	((calling_convention) << ARM_SMCCC_CALL_CONV_SHIFT) | \
	(((owner) & ARM_SMCCC_OWNER_MASK) << ARM_SMCCC_OWNER_SHIFT) | \
	((func_num) & ARM_SMCCC_FUNC_MASK))

/**
 * struct sandbox_smccc_1_2_regs - Arguments for or Results from emulated SMC call
 * @a0-a17 argument values from registers 0 to 17
 */
struct sandbox_smccc_1_2_regs {
	unsigned long a0;
	unsigned long a1;
	unsigned long a2;
	unsigned long a3;
	unsigned long a4;
	unsigned long a5;
	unsigned long a6;
	unsigned long a7;
	unsigned long a8;
	unsigned long a9;
	unsigned long a10;
	unsigned long a11;
	unsigned long a12;
	unsigned long a13;
	unsigned long a14;
	unsigned long a15;
	unsigned long a16;
	unsigned long a17;
};

typedef struct sandbox_smccc_1_2_regs ffa_value_t;

/* UUIDs strings of services emulated by the FF-A sandbox driver */
#define SANDBOX_SERVICE1_UUID	"ed32d533-4209-99e6-2d72-cdd998a79cc0"
#define SANDBOX_SERVICE2_UUID	"ed32d544-4209-99e6-2d72-cdd998a79cc0"

/* IDs of secure partitions (SPs) emulated by the FF-A sandbox driver */
#define SANDBOX_SP1_ID 0x1245
#define SANDBOX_SP2_ID 0x9836
#define SANDBOX_SP3_ID 0x6452
#define SANDBOX_SP4_ID 0x7814

/* Invalid service UUID (no matching SP) */
#define SANDBOX_SERVICE3_UUID	"55d532ed-0942-e699-722d-c09ca798d9cd"

/* Invalid service UUID (invalid UUID string format) */
#define SANDBOX_SERVICE4_UUID	"32ed-0942-e699-722d-c09ca798d9cd"

/* Number of valid services */
#define SANDBOX_SP_COUNT_PER_VALID_SERVICE	2

/**
 * struct ffa_sandbox_data - generic data structure used to exchange
 *						data between test cases and the sandbox driver
 * @data0_size:	size of the first argument
 * @data0:	pointer to the first argument
 * @data1_size>:	size of the second argument
 * @data1:	pointer to the second argument
 *
 * Using this structure sandbox test cases can pass various types of data with different sizes.
 */
struct ffa_sandbox_data {
	u32 data0_size; /* size of the first argument */
	void *data0; /* pointer to the first argument */
	u32 data1_size; /* size of the second argument */
	void *data1; /* pointer to the second argument */
};

/* The sandbox driver public functions */

/**
 * sandbox_ffa_query_core_state() - The driver dispatcher function
 * @dev:	the sandbox_arm_ffa device
 * @queried_func_id:	The FF-A function to be queried
 * @func_data:  Pointer to the FF-A function arguments container structure
 * Return:
 * 0 on success. Otherwise, failure
 */
int sandbox_ffa_query_core_state(struct udevice *dev, u32 queried_func_id,
				 struct ffa_sandbox_data *func_data);

/**
 * sandbox_arm_ffa_smccc_smc() - FF-A SMC call emulation
 * @args:	the SMC call arguments
 * @res:	the SMC call returned data
 * Return:
 * 0 on success. Otherwise, failure.
 * FF-A protocol error codes are returned using the registers arguments as described
 * by the specification
 */
void sandbox_arm_ffa_smccc_smc(ffa_value_t args, ffa_value_t *res);

#endif
