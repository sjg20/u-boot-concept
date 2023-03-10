// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2022-2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * Authors:
 *   Abdellatif El Khlifi <abdellatif.elkhlifi@arm.com>
 */

#include <common.h>
#include <arm_ffa.h>
#include <command.h>
#include <dm.h>
#include <mapmem.h>
#include <stdlib.h>
#include <asm/io.h>

/**
 * do_ffa_getpart() - implementation of the getpart subcommand
 * @cmdtp:		Command Table
 * @flag:		flags
 * @argc:		number of arguments
 * @argv:		arguments
 *
 * This function queries the secure partition information which the UUID is provided
 * as an argument. The function uses the arm_ffa driver partition_info_get operation
 * which implements FFA_PARTITION_INFO_GET ABI to retrieve the data.
 * The input UUID string is expected to be in big endian format.
 *
 * Return:
 *
 * CMD_RET_SUCCESS: on success, otherwise failure
 */
static int do_ffa_getpart(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	u32 count = 0;
	int ret;
	struct ffa_partition_info *parts_info;
	u32 info_idx;
	struct udevice *dev = NULL;
	struct ffa_bus_ops *ffa_ops = NULL;

	if (argc != 1)
		return -EINVAL;

	uclass_get_device_by_name(UCLASS_FFA, "arm_ffa", &dev);
	if (!dev) {
		log_err("[FFA] Cannot find FF-A bus device\n");
		return -ENODEV;
	}

	ffa_ops = (struct ffa_bus_ops *)ffa_bus_get_ops(dev);
	if (!ffa_ops) {
		log_err("[FFA] Invalid FF-A ops\n");
		return -EINVAL;
	}

	/* Mode 1: getting the number of secure partitions */
	ret = ffa_ops->partition_info_get(dev, argv[0], &count, NULL);
	if (ret != 0) {
		log_err("[FFA] Failure in querying partitions count (error code: %d)\n", ret);
		return ret;
	}

	if (!count) {
		log_info("[FFA] No secure partition found\n");
		return ret;
	}

	/*
	 * Pre-allocate a buffer to be filled by the driver
	 * with ffa_partition_info structs
	 */

	log_info("[FFA] Pre-allocating %d partition(s) info structures\n", count);

	parts_info = calloc(count, sizeof(struct ffa_partition_info));
	if (!parts_info)
		return -EINVAL;

	/* Ask the driver to fill the buffer with the SPs info */

	ret = ffa_ops->partition_info_get(dev, argv[0], &count, parts_info);
	if (ret != 0) {
		log_err("[FFA] Failure in querying partition(s) info (error code: %d)\n", ret);
		free(parts_info);
		return ret;
	}

	/* SPs found , show the partition information */
	for (info_idx = 0; info_idx < count ; info_idx++) {
		log_info("[FFA] Partition: id = 0x%x , exec_ctxt 0x%x , properties 0x%x\n",
			 parts_info[info_idx].id,
			 parts_info[info_idx].exec_ctxt,
			 parts_info[info_idx].properties);
	}

	free(parts_info);

	return 0;
}

/**
 * do_ffa_ping() - implementation of the ping subcommand
 * @cmdtp:		Command Table
 * @flag:		flags
 * @argc:		number of arguments
 * @argv:		arguments
 *
 * This function sends data to the secure partition which the ID is provided
 * as an argument. The function uses the arm_ffa driver sync_send_receive operation
 * which implements FFA_MSG_SEND_DIRECT_{REQ,RESP} ABIs to send/receive data.
 *
 * Return:
 *
 * CMD_RET_SUCCESS: on success, otherwise failure
 */
int do_ffa_ping(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct ffa_send_direct_data msg = {
			.data0 = 0xaaaaaaaa,
			.data1 = 0xbbbbbbbb,
			.data2 = 0xcccccccc,
			.data3 = 0xdddddddd,
			.data4 = 0xeeeeeeee,
	};
	u16 part_id;
	int ret;
	struct udevice *dev = NULL;
	struct ffa_bus_ops *ffa_ops = NULL;

	if (argc != 1)
		return -EINVAL;

	errno = 0;
	part_id = strtoul(argv[0], NULL, 16);

	if (errno) {
		log_err("[FFA] Invalid partition ID\n");
		return -EINVAL;
	}

	uclass_get_device_by_name(UCLASS_FFA, "arm_ffa", &dev);
	if (!dev) {
		log_err("[FFA] Cannot find FF-A bus device\n");
		return -ENODEV;
	}

	ffa_ops = (struct ffa_bus_ops *)ffa_bus_get_ops(dev);
	if (!ffa_ops) {
		log_err("[FFA] Invalid FF-A ops\n");
		return -EINVAL;
	}

	ret = ffa_ops->sync_send_receive(dev, part_id, &msg, 1);
	if (!ret) {
		u8 cnt;

		log_info("[FFA] SP response:\n[LSB]\n");
		for (cnt = 0;
		     cnt < sizeof(struct ffa_send_direct_data) / sizeof(u64);
		     cnt++)
			log_info("[FFA] 0x%llx\n", ((u64 *)&msg)[cnt]);
	} else {
		log_err("[FFA] Sending direct request error (%d)\n", ret);
	}

	return ret;
}

/**
 *do_ffa_devlist() - implementation of the devlist subcommand
 * @cmdtp: [in]		Command Table
 * @flag:		flags
 * @argc:		number of arguments
 * @argv:		arguments
 *
 * This function queries the devices belonging to the UCLASS_FFA
 * class.
 *
 * Return:
 *
 * CMD_RET_SUCCESS: on success, otherwise failure
 */
int do_ffa_devlist(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct udevice *dev = NULL;
	int i;

	log_info("[FFA] FF-A uclass entries:\n");

	for (i = 0, uclass_first_device(UCLASS_FFA, &dev);
	     dev;
	     uclass_next_device(&dev), i++) {
		log_info("[FFA] entry %d - instance %08x, ops %08x, plat %08x\n",
			 i,
			 (u32)map_to_sysmem(dev),
			 (u32)map_to_sysmem(dev->driver->ops),
			 (u32)map_to_sysmem(dev_get_plat(dev)));
	}

	return 0;
}

static struct cmd_tbl armffa_commands[] = {
	U_BOOT_CMD_MKENT(getpart, 1, 1, do_ffa_getpart, "", ""),
	U_BOOT_CMD_MKENT(ping, 1, 1, do_ffa_ping, "", ""),
	U_BOOT_CMD_MKENT(devlist, 0, 1, do_ffa_devlist, "", ""),
};

/**
 * do_armffa() - the armffa command main function
 * @cmdtp:	Command Table
 * @flag:		flags
 * @argc:		number of arguments
 * @argv:		arguments
 *
 * This function identifies which armffa subcommand to run.
 * Then, it makes sure the arm_ffa device is probed and
 * ready for use.
 * Then, it runs the subcommand.
 *
 * Return:
 *
 * CMD_RET_SUCCESS: on success, otherwise failure
 */
static int do_armffa(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct cmd_tbl *armffa_cmd;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	armffa_cmd = find_cmd_tbl(argv[1], armffa_commands, ARRAY_SIZE(armffa_commands));

	argc -= 2;
	argv += 2;

	if (!armffa_cmd || argc > armffa_cmd->maxargs)
		return CMD_RET_USAGE;

	if (IS_ENABLED(CONFIG_SANDBOX_FFA)) {
		struct udevice *sdx_dev = NULL;
		/*  Probe the FF-A sandbox driver, then bind the FF-A bus driver */
		uclass_get_device_by_name(UCLASS_FFA, "sandbox_arm_ffa", &sdx_dev);
		if (!sdx_dev) {
			log_err("[FFA] Cannot find FF-A sandbox device\n");
			return -ENODEV;
		}
	}

	ret = armffa_cmd->cmd(armffa_cmd, flag, argc, argv);

	return cmd_process_error(armffa_cmd, ret);
}

U_BOOT_CMD(armffa, 4, 1, do_armffa,
	   "Arm FF-A operations test command",
	   "getpart <partition UUID>\n"
	   "	 - lists the partition(s) info\n"
	   "ping <partition ID>\n"
	   "	 - sends a data pattern to the specified partition\n"
	   "devlist\n"
	   "	 - displays instance info of FF-A devices (the bus and its associated sandbox\n");
