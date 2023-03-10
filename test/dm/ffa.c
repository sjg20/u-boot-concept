// SPDX-License-Identifier: GPL-2.0+
/*
 * Functional tests for UCLASS_FFA  class
 *
 * Copyright 2022-2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * Authors:
 *   Abdellatif El Khlifi <abdellatif.elkhlifi@arm.com>
 */

#include <common.h>
#include <console.h>
#include <dm.h>
#include <sandbox_arm_ffa.h>
#include "../../drivers/firmware/arm-ffa/sandbox_arm_ffa_priv.h"
#include <dm/test.h>
#include <test/test.h>
#include <test/ut.h>

/* Macros */

#define LOG_MSG_SZ (100)
#define LOG_CMD_SZ (LOG_MSG_SZ * 2)

/* Functional tests for the UCLASS_FFA */

static int dm_test_ffa_log(struct unit_test_state *uts, char *msg)
{
	char cmd[LOG_CMD_SZ] = {0};

	console_record_reset();

	snprintf(cmd, LOG_CMD_SZ, "echo \"%s\"", msg);
	run_command(cmd, 0);

	ut_assert_console_end();

	return 0;
}

static int check_fwk_version(struct ffa_priv *priv, struct sandbox_ffa_priv *sdx_priv,
			     struct unit_test_state *uts)
{
	if (priv->dscvry_info.fwk_version != sdx_priv->fwk_version) {
		char msg[LOG_MSG_SZ] = {0};

		snprintf(msg, LOG_MSG_SZ,
			 "[%s]: Error: framework version: core = 0x%x , sandbox  = 0x%x", __func__,
			 priv->dscvry_info.fwk_version,
			sdx_priv->fwk_version);

		dm_test_ffa_log(uts, msg);
		return CMD_RET_FAILURE;
	}
	return 0;
}

static int check_endpoint_id(struct ffa_priv *priv, struct unit_test_state *uts)
{
	if (priv->id) {
		char msg[LOG_MSG_SZ] = {0};

		snprintf(msg, LOG_MSG_SZ,
			 "[%s]: Error: endpoint id: core = 0x%x", __func__, priv->id);
		dm_test_ffa_log(uts, msg);
		return CMD_RET_FAILURE;
	}
	return 0;
}

static int check_rxtxbuf(struct ffa_priv *priv, struct unit_test_state *uts)
{
	if (!priv->pair.rxbuf && priv->pair.txbuf) {
		char msg[LOG_MSG_SZ] = {0};

		snprintf(msg, LOG_MSG_SZ, "[%s]: Error: rxbuf = %p txbuf = %p", __func__,
			 priv->pair.rxbuf,
			 priv->pair.txbuf);
		dm_test_ffa_log(uts, msg);
		return CMD_RET_FAILURE;
	}
	return 0;
}

static int check_features(struct ffa_priv *priv, struct unit_test_state *uts)
{
	char msg[LOG_MSG_SZ] = {0};

	if (priv->pair.rxtx_min_pages != RXTX_4K &&
	    priv->pair.rxtx_min_pages != RXTX_16K &&
	    priv->pair.rxtx_min_pages != RXTX_64K) {
		snprintf(msg,
			 LOG_MSG_SZ,
			 "[%s]: Error: FFA_RXTX_MAP features = 0x%lx",
			 __func__,
			 priv->pair.rxtx_min_pages);
		dm_test_ffa_log(uts, msg);
		return CMD_RET_FAILURE;
	}

	return 0;
}

static int check_rxbuf_mapped_flag(u32 queried_func_id,
				   u8 rxbuf_mapped,
				   struct unit_test_state *uts)
{
	char msg[LOG_MSG_SZ] = {0};

	switch (queried_func_id) {
	case FFA_RXTX_MAP:
	{
		if (rxbuf_mapped)
			return 0;
		break;
	}
	case FFA_RXTX_UNMAP:
	{
		if (!rxbuf_mapped)
			return 0;
		break;
	}
	default:
		return CMD_RET_FAILURE;
	}

	snprintf(msg, LOG_MSG_SZ, "[%s]: Error: %s mapping issue", __func__,
		 (queried_func_id == FFA_RXTX_MAP ? "FFA_RXTX_MAP" : "FFA_RXTX_UNMAP"));
	dm_test_ffa_log(uts, msg);

	return CMD_RET_FAILURE;
}

static int check_rxbuf_release_flag(u8 rxbuf_owned, struct unit_test_state *uts)
{
	if (rxbuf_owned) {
		char msg[LOG_MSG_SZ] = {0};

		snprintf(msg, LOG_MSG_SZ, "[%s]: Error: RX buffer not released", __func__);
		dm_test_ffa_log(uts, msg);
		return CMD_RET_FAILURE;
	}
	return 0;
}

static int  test_ffa_msg_send_direct_req(u16 part_id, struct unit_test_state *uts)
{
	struct ffa_send_direct_data msg = {0};
	u8 cnt;
	struct udevice *ffa_dev = NULL;
	struct ffa_bus_ops *ffa_ops = NULL;

	uclass_get_device_by_name(UCLASS_FFA, "arm_ffa", &ffa_dev);
	ut_assertok(!ffa_dev);

	ffa_ops = (struct ffa_bus_ops *)ffa_bus_get_ops(ffa_dev);
	ut_assertok(!ffa_ops);

	ut_assertok(ffa_ops->sync_send_receive(ffa_dev, part_id, &msg, 1));

	for (cnt = 0; cnt < sizeof(struct ffa_send_direct_data) / sizeof(u64); cnt++)
		ut_assertok(((u64 *)&msg)[cnt] != 0xffffffffffffffff);

	return 0;
}

static int test_partitions_and_comms(const char *service_uuid,
				     struct sandbox_ffa_priv *sdx_priv,
				     struct unit_test_state *uts)
{
	u32 count = 0;
	struct ffa_partition_info *parts_info;
	u32 info_idx, exp_info_idx;
	int ret;
	struct udevice *ffa_dev = NULL;
	struct ffa_bus_ops *ffa_ops = NULL;

	uclass_get_device_by_name(UCLASS_FFA, "arm_ffa", &ffa_dev);
	ut_assertok(!ffa_dev);

	ffa_ops = (struct ffa_bus_ops *)ffa_bus_get_ops(ffa_dev);
	ut_assertok(!ffa_ops);

	/* Get from the driver the count of the SPs matching the UUID */
	ret = ffa_ops->partition_info_get(ffa_dev, service_uuid, &count, NULL);
	/* Make sure partitions are detected */
	ut_assertok(ret != 0);
	ut_assertok(count != SANDBOX_SP_COUNT_PER_VALID_SERVICE);

	/* Pre-allocate a buffer to be filled by the driver with ffa_partition_info structs */

	parts_info = calloc(count, sizeof(struct ffa_partition_info));
	ut_assertok(!parts_info);

	/* Ask the driver to fill the buffer with the SPs info */
	ret = ffa_ops->partition_info_get(ffa_dev, service_uuid, &count, parts_info);
	if (ret != 0) {
		free(parts_info);
		ut_assertok(ret != 0);
	}

	/* SPs found , verify the partitions information */

	ret = CMD_RET_FAILURE;

	for (info_idx = 0; info_idx < count ; info_idx++) {
		for (exp_info_idx = 0;
		     exp_info_idx < sdx_priv->partitions.count;
		     exp_info_idx++) {
			if (parts_info[info_idx].id ==
			   sdx_priv->partitions.descs[exp_info_idx].info.id) {
				ret = memcmp(&parts_info[info_idx],
					     &sdx_priv->partitions.descs[exp_info_idx]
					     .info,
					     sizeof(struct ffa_partition_info));
				if (ret)
					free(parts_info);
				ut_assertok(ret != 0);
				/* Send and receive data from the current partition */
				test_ffa_msg_send_direct_req(parts_info[info_idx].id, uts);
			}
			ret = 0;
		}
	}

	free(parts_info);

	/* Verify expected partitions found in the emulated secure world */
	ut_assertok(ret != 0);

	return 0;
}

static int dm_test_ffa_ack(struct unit_test_state *uts)
{
	struct ffa_priv *priv = NULL;
	struct sandbox_ffa_priv *sdx_priv = NULL;
	struct ffa_sandbox_data func_data = {0};
	u8 rxbuf_flag = 0;
	const char *svc1_uuid = SANDBOX_SERVICE1_UUID;
	const char *svc2_uuid = SANDBOX_SERVICE2_UUID;
	int ret;
	struct udevice *ffa_dev = NULL, *sdx_dev = NULL;
	struct ffa_bus_ops *ffa_ops = NULL;

	/* Test probing the FF-A sandbox driver, then binding the FF-A bus driver */
	uclass_get_device_by_name(UCLASS_FFA, "sandbox_arm_ffa", &sdx_dev);
	ut_assertok(!sdx_dev);

	/* Test probing the FF-A bus driver */
	uclass_get_device_by_name(UCLASS_FFA, "arm_ffa", &ffa_dev);
	ut_assertok(!ffa_dev);

	ffa_ops = (struct ffa_bus_ops *)ffa_bus_get_ops(ffa_dev);
	ut_assertok(!ffa_ops);

	/* Get a pointer to the FF-A core and sandbox drivers private data */
	priv = dev_get_priv(ffa_dev);
	sdx_priv = dev_get_priv(sdx_dev);

	/* Make sure private data pointers are retrieved */
	ut_assertok(priv == 0);
	ut_assertok(sdx_priv == 0);

	/* Test FFA_VERSION */
	ut_assertok(check_fwk_version(priv, sdx_priv, uts));

	/* Test FFA_ID_GET */
	ut_assertok(check_endpoint_id(priv, uts));

	/* Test FFA_FEATURES */
	ut_assertok(check_features(priv, uts));

	/*  Test core RX/TX buffers */
	ut_assertok(check_rxtxbuf(priv, uts));

	/* Test FFA_RXTX_MAP */
	func_data.data0 = &rxbuf_flag;
	func_data.data0_size = sizeof(rxbuf_flag);

	rxbuf_flag = 0;
	ut_assertok(sandbox_ffa_query_core_state(sdx_dev, FFA_RXTX_MAP, &func_data));
	ut_assertok(check_rxbuf_mapped_flag(FFA_RXTX_MAP, rxbuf_flag, uts));

	/* FFA_PARTITION_INFO_GET / FFA_MSG_SEND_DIRECT_REQ */
	ret = test_partitions_and_comms(svc1_uuid, sdx_priv, uts);
	ut_assertok(ret != 0);

	/* Test FFA_RX_RELEASE */
	rxbuf_flag = 1;
	ut_assertok(sandbox_ffa_query_core_state(sdx_dev, FFA_RX_RELEASE, &func_data));
	ut_assertok(check_rxbuf_release_flag(rxbuf_flag, uts));

	/* FFA_PARTITION_INFO_GET / FFA_MSG_SEND_DIRECT_REQ */
	ret = test_partitions_and_comms(svc2_uuid, sdx_priv, uts);
	ut_assertok(ret != 0);

	/* Test FFA_RX_RELEASE */
	rxbuf_flag = 1;
	ut_assertok(sandbox_ffa_query_core_state(sdx_dev, FFA_RX_RELEASE, &func_data));
	ut_assertok(check_rxbuf_release_flag(rxbuf_flag, uts));

	/* Test FFA_RXTX_UNMAP */
	ut_assertok(ffa_ops->rxtx_unmap(ffa_dev));

	rxbuf_flag = 1;
	ut_assertok(sandbox_ffa_query_core_state(sdx_dev, FFA_RXTX_UNMAP, &func_data));
	ut_assertok(check_rxbuf_mapped_flag(FFA_RXTX_UNMAP, rxbuf_flag, uts));

	return 0;
}

DM_TEST(dm_test_ffa_ack, UT_TESTF_SCAN_FDT | UT_TESTF_CONSOLE_REC);

static int dm_test_ffa_nack(struct unit_test_state *uts)
{
	struct ffa_priv *priv = NULL;
	struct sandbox_ffa_priv *sdx_priv = NULL;
	const char *valid_svc_uuid = SANDBOX_SERVICE1_UUID;
	const char *unvalid_svc_uuid = SANDBOX_SERVICE3_UUID;
	const char *unvalid_svc_uuid_str = SANDBOX_SERVICE4_UUID;
	struct ffa_send_direct_data msg = {0};
	int ret;
	u32 count = 0;
	u16 part_id = 0;
	struct udevice *ffa_dev = NULL, *sdx_dev = NULL;
	struct ffa_bus_ops *ffa_ops = NULL;

	/* Test probing the FF-A sandbox driver, then binding the FF-A bus driver */
	uclass_get_device_by_name(UCLASS_FFA, "sandbox_arm_ffa", &sdx_dev);
	ut_assertok(!sdx_dev);

	/* Test probing the  FF-A bus driver */
	uclass_get_device_by_name(UCLASS_FFA, "arm_ffa", &ffa_dev);
	ut_assertok(!ffa_dev);

	ffa_ops = (struct ffa_bus_ops *)ffa_bus_get_ops(ffa_dev);
	ut_assertok(!ffa_ops);

	/* Get a pointer to the FF-A core and sandbox drivers private data */
	priv = dev_get_priv(ffa_dev);
	sdx_priv = dev_get_priv(sdx_dev);

	/* Make sure private data pointers are retrieved */
	ut_assertok(priv == 0);
	ut_assertok(sdx_priv == 0);

	/* Query partitions count using  invalid arguments */
	ret = ffa_ops->partition_info_get(ffa_dev, unvalid_svc_uuid, NULL, NULL);
	ut_assertok(ret != -EINVAL);

	/* Query partitions count using an invalid UUID  string */
	ret = ffa_ops->partition_info_get(ffa_dev, unvalid_svc_uuid_str, &count, NULL);
	ut_assertok(ret != -EINVAL);

	/* Query partitions count using an invalid UUID (no matching SP) */
	count = 0;
	ret = ffa_ops->partition_info_get(ffa_dev, unvalid_svc_uuid, &count, NULL);
	ut_assertok(count != 0);

	/* Query partitions count using a valid UUID */
	count = 0;
	ret = ffa_ops->partition_info_get(ffa_dev, valid_svc_uuid, &count, NULL);
	/* Make sure partitions are detected */
	ut_assertok(ret != 0);
	ut_assertok(count != SANDBOX_SP_COUNT_PER_VALID_SERVICE);

	/* Send data to an invalid partition */
	ret = ffa_ops->sync_send_receive(ffa_dev, part_id, &msg, 1);
	ut_assertok(ret != -EINVAL);

	/* Send data to a valid partition */
	part_id = priv->partitions.descs[0].info.id;
	ret = ffa_ops->sync_send_receive(ffa_dev, part_id, &msg, 1);
	ut_assertok(ret != 0);

	return 0;
}

DM_TEST(dm_test_ffa_nack, UT_TESTF_SCAN_FDT | UT_TESTF_CONSOLE_REC);
