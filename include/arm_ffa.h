/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2022-2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * Authors:
 *   Abdellatif El Khlifi <abdellatif.elkhlifi@arm.com>
 */

#ifndef __ARM_FFA_H
#define __ARM_FFA_H

#include <linux/printk.h>

/*
 * This header is public. It can be used by clients to access
 * data structures and definitions they need
 */

/*
 * struct ffa_partition_info - Partition information descriptor
 * @id:	Partition ID
 * @exec_ctxt:	Execution context count
 * @properties:	Partition properties
 *
 * Data structure containing information about partitions instantiated in the system
 * This structure is filled with the data queried by FFA_PARTITION_INFO_GET
 */
struct ffa_partition_info {
	u16 id;
	u16 exec_ctxt;
/* partition supports receipt of direct requests */
#define FFA_PARTITION_DIRECT_RECV	BIT(0)
/* partition can send direct requests. */
#define FFA_PARTITION_DIRECT_SEND	BIT(1)
/* partition can send and receive indirect messages. */
#define FFA_PARTITION_INDIRECT_MSG	BIT(2)
	u32 properties;
};

/*
 * struct ffa_send_direct_data - Data structure hosting the data
 *                                       used by FFA_MSG_SEND_DIRECT_{REQ,RESP}
 * @data0-4:	Data read/written from/to x3-x7 registers
 *
 * Data structure containing the data to be sent by FFA_MSG_SEND_DIRECT_REQ
 * or read from FFA_MSG_SEND_DIRECT_RESP
 */

/* For use with FFA_MSG_SEND_DIRECT_{REQ,RESP} which pass data via registers */
struct ffa_send_direct_data {
	unsigned long data0; /* w3/x3 */
	unsigned long data1; /* w4/x4 */
	unsigned long data2; /* w5/x5 */
	unsigned long data3; /* w6/x6 */
	unsigned long data4; /* w7/x7 */
};

struct udevice;

/**
 * struct ffa_bus_ops - The driver operations structure
 * @partition_info_get:	callback for the FFA_PARTITION_INFO_GET
 * @sync_send_receive:	callback for the FFA_MSG_SEND_DIRECT_REQ
 * @rxtx_unmap:	callback for the FFA_RXTX_UNMAP
 *
 * The data structure providing all the operations supported by the driver.
 * This structure is EFI runtime resident.
 */
struct ffa_bus_ops {
	int (*partition_info_get)(struct udevice *dev, const char *uuid_str,
				  u32 *sp_count, struct ffa_partition_info *buffer);
	int (*sync_send_receive)(struct udevice *dev, u16 dst_part_id,
				 struct ffa_send_direct_data *msg,
				 bool is_smc64);
	int (*rxtx_unmap)(struct udevice *dev);
};

/* The device driver and the Uclass driver public functions */

/**
 * ffa_bus_get_ops() - bus driver operations getter
 * @dev:	the arm_ffa device
 * Returns a pointer to the FF-A driver ops field.
 * Return:
 * The ops pointer on success, NULL on failure.
 */
const struct ffa_bus_ops *ffa_bus_get_ops(struct udevice *dev);
#endif
