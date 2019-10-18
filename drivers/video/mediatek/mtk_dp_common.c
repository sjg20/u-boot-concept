// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek dp common entrance and function
 *
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Tommy Chen <tommyyl.chen@mediatek.com>
 */

#include <string.h>
#include <asm/io.h>
#include <asm/system.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <video.h>
#include <video_bridge.h>
#include <linux/delay.h>

#include "mtk_dp_common.h"
#include "mtk_dp_hal.h"
#include "mtk_dp_intf.h"
#include "mtk_edp_panel.h"

struct display_mode g_mode;

/*
 * use MTK_DPTX_DEBUG to enable dp debug logs
 * #define MTK_DPTX_DEBUG
 */

#ifdef MTK_DPTX_DEBUG
#define dptx_printf(string, args...) printf("[DPTX]"string, ##args)
#else
#define dptx_printf(string, args...)
#endif

static bool mtk_dptx_aux_write_dpcd(struct mtk_dp *mtk_dp, u8 cmd, u32 dpcd_addr, size_t length,
				    u8 *data)
{
	bool ret = true;
	size_t times = 0;
	size_t remain = 0;
	size_t loop = 0;

	if (length > DP_AUX_MAX_PAYLOAD_BYTES) {
		times = length / DP_AUX_MAX_PAYLOAD_BYTES;
		remain = length % DP_AUX_MAX_PAYLOAD_BYTES;

		for (loop = 0; loop < times; loop++)
			ret &= mtk_dptx_hal_aux_do_transfer(mtk_dp, false,
				cmd,
				dpcd_addr + (loop * DP_AUX_MAX_PAYLOAD_BYTES),
				data + (loop * DP_AUX_MAX_PAYLOAD_BYTES),
				DP_AUX_MAX_PAYLOAD_BYTES);
		if (remain > 0)
			ret &= mtk_dptx_hal_aux_do_transfer(mtk_dp, false,
				cmd,
				dpcd_addr + (times * DP_AUX_MAX_PAYLOAD_BYTES),
				data + (times * DP_AUX_MAX_PAYLOAD_BYTES),
				remain);
	} else {
		ret &= mtk_dptx_hal_aux_do_transfer(mtk_dp, false,
				cmd,
				dpcd_addr,
				data,
				length);
	}

	return ret;
}

static bool mtk_dptx_aux_read_dpcd(struct mtk_dp *mtk_dp, u8 cmd, u32 dpcd_addr, size_t length,
				   u8 *rx_buf)
{
	bool ret = true;
	size_t times = 0;
	size_t remain = 0;
	size_t loop = 0;

	if (length > DP_AUX_MAX_PAYLOAD_BYTES) {
		times = length / DP_AUX_MAX_PAYLOAD_BYTES;
		remain = length % DP_AUX_MAX_PAYLOAD_BYTES;

		for (loop = 0; loop < times; loop++)
			ret &= mtk_dptx_hal_aux_do_transfer(mtk_dp, true,
				cmd,
				dpcd_addr + (loop * DP_AUX_MAX_PAYLOAD_BYTES),
				rx_buf + (loop * DP_AUX_MAX_PAYLOAD_BYTES),
				DP_AUX_MAX_PAYLOAD_BYTES);

		if (remain > 0)
			ret &= mtk_dptx_hal_aux_do_transfer(mtk_dp, true,
				cmd,
				dpcd_addr + (times * DP_AUX_MAX_PAYLOAD_BYTES),
				rx_buf + (times * DP_AUX_MAX_PAYLOAD_BYTES),
				remain);
	} else {
		ret &= mtk_dptx_hal_aux_do_transfer(mtk_dp, true,
				cmd,
				dpcd_addr,
				rx_buf,
				length);
	}

	return ret;
}

unsigned char edid_data[EDID_SIZE];	/* 4 block 512 Bytes */

int mtk_dp_get_edid(struct mtk_dp *mtk_dp)
{
	int ret;
	u8 tmp;
	u8 ext_block_no;
	u8 i;

	memset(edid_data, 0, EDID_SIZE);

	tmp = 0;
	mtk_dptx_aux_write_dpcd(mtk_dp, DP_AUX_I2C_WRITE, 0x50, 0x1, &tmp);

	for (tmp = 0; tmp < ONE_BLOCK_SIZE / DP_AUX_MAX_PAYLOAD_BYTES; tmp++)
		mtk_dptx_aux_read_dpcd(mtk_dp, DP_AUX_I2C_READ, 0x50, DP_AUX_MAX_PAYLOAD_BYTES,
				       edid_data + tmp * 16);

	ext_block_no = edid_data[EDID_ADDR_EXT_BLOCK_FLAG];
	if (ext_block_no > 0) {
		for (i = 1; i <= ext_block_no; i++) {
			dptx_printf("%s ext_block_no = %d\n", __func__, ext_block_no);
			for (tmp = ONE_BLOCK_SIZE / DP_AUX_MAX_PAYLOAD_BYTES;
			     tmp < (ONE_BLOCK_SIZE * (i + 1)) / DP_AUX_MAX_PAYLOAD_BYTES; tmp++)
				mtk_dptx_aux_read_dpcd(mtk_dp, DP_AUX_I2C_READ, 0x50,
						       DP_AUX_MAX_PAYLOAD_BYTES,
						       edid_data + tmp * 16);
		}
	}

	return 0;
}

static u8 dp_link_status(const u8 link_status[DP_LINK_STATUS_SIZE], int r)
{
	return link_status[r - DP_LANE0_1_STATUS];
}

static u8 dp_get_lane_status(const u8 link_status[DP_LINK_STATUS_SIZE],
			     int lane)
{
	int i = DP_LANE0_1_STATUS + (lane >> 1);
	int s = (lane & 1) * 4;
	u8 l = dp_link_status(link_status, i);

	return (l >> s) & 0xf;
}

static bool drm_dp_clock_recovery_ok(const u8 link_status[DP_LINK_STATUS_SIZE], int lane_count)
{
	int lane;
	u8 lane_status;

	for (lane = 0; lane < lane_count; lane++) {
		lane_status = dp_get_lane_status(link_status, lane);
		if ((lane_status & DP_LANE_CR_DONE) == 0)
			return false;
	}
	return true;
}

static void drm_dp_link_train_clock_recovery_delay(const u8 dpcd[DP_RECEIVER_CAP_SIZE])
{
	unsigned long rd_interval = dpcd[DP_TRAINING_AUX_RD_INTERVAL] &
					 DP_TRAINING_AUX_RD_MASK;

	if (rd_interval > 4)
		printf("AUX interval %lu, out of range (max 4)\n", rd_interval);

	if (rd_interval == 0 || dpcd[DP_DPCD_REV] >= DP_DPCD_REV_14)
		rd_interval = 1;
	else
		rd_interval *= 4;

	mdelay(rd_interval);
}

static void __drm_dp_link_train_channel_eq_delay(unsigned long rd_interval)
{
	if (rd_interval > 4)
		printf("AUX interval %lu, out of range (max 4)\n", rd_interval);

	if (rd_interval == 0)
		rd_interval = 1;
	else
		rd_interval *= 4;

	mdelay(rd_interval);
}

static void drm_dp_link_train_channel_eq_delay(const u8 dpcd[DP_RECEIVER_CAP_SIZE])
{
	__drm_dp_link_train_channel_eq_delay(dpcd[DP_TRAINING_AUX_RD_INTERVAL] &
					     DP_TRAINING_AUX_RD_MASK);
}

static bool drm_dp_channel_eq_ok(const u8 link_status[DP_LINK_STATUS_SIZE], int lane_count)
{
	u8 lane_align;
	u8 lane_status;
	int lane;

	lane_align = dp_link_status(link_status, DP_LANE_ALIGN_STATUS_UPDATED);
	if ((lane_align & DP_INTERLANE_ALIGN_DONE) == 0)
		return false;
	for (lane = 0; lane < lane_count; lane++) {
		lane_status = dp_get_lane_status(link_status, lane);
		if ((lane_status & DP_CHANNEL_EQ_BITS) != DP_CHANNEL_EQ_BITS)
			return false;
	}
	return true;
}

static void mtk_dptx_video_mute(struct mtk_dp *mtk_dp, bool enable)
{
	mtk_dptx_hal_video_mute(mtk_dp, enable);
}

static void mtk_dptx_set_sdp_down_cnt_init(struct mtk_dp *mtk_dp, u16 sram_read_start)
{
	u16 sdp_down_cnt_init = 0x0000;
	u8 dc_offset;

	if (mtk_dp->mode->clock > 0)
		sdp_down_cnt_init = (sram_read_start *
			mtk_dp->training_info.link_rate * 2700 * 8)
			/ (mtk_dp->mode->clock * 4);

	switch (mtk_dp->training_info.link_lane_count) {
	case DP_LANECOUNT_1:
		sdp_down_cnt_init = (sdp_down_cnt_init > 0x1A) ?
			sdp_down_cnt_init : 0x1A;
		break;
	case DP_LANECOUNT_2:
		dc_offset = (mtk_dp->mode->vtotal <= 525) ?
			0x04 : 0x00;
		sdp_down_cnt_init = (sdp_down_cnt_init > 0x10) ?
			sdp_down_cnt_init : 0x10 + dc_offset;
		break;
	case DP_LANECOUNT_4:
		sdp_down_cnt_init = (sdp_down_cnt_init > 0x06) ?
			sdp_down_cnt_init : 0x06;
		break;
	default:
		sdp_down_cnt_init = (sdp_down_cnt_init > 0x06) ?
			sdp_down_cnt_init : 0x06;
		break;
	}

	dptx_printf("PixRateKhz = %d SDP_DC_Init = %x\n", mtk_dp->mode->clock, sdp_down_cnt_init);
	mtk_dptx_hal_set_sdp_down_cnt_init(mtk_dp, sdp_down_cnt_init);
}

static void mtk_dptx_set_sdp_down_cnt_init_in_hblanking(struct mtk_dp *mtk_dp)
{
	int pix_clk_mhz = mtk_dp->mode->clock / 1000;
	u8 dc_offset;

	switch (mtk_dp->training_info.link_lane_count) {
	case DP_LANECOUNT_1:
		mtk_dptx_hal_set_sdp_down_cnt_init_in_hblanking(mtk_dp, 0x0020);
		break;
	case DP_LANECOUNT_2:
		dc_offset = (mtk_dp->mode->vtotal <= 525) ? 0x14 : 0x00;
		mtk_dptx_hal_set_sdp_down_cnt_init_in_hblanking(mtk_dp, 0x0018 + dc_offset);
		break;
	case DP_LANECOUNT_4:
		dc_offset = (mtk_dp->mode->vtotal <= 525) ? 0x08 : 0x00;
		if (pix_clk_mhz > (mtk_dp->training_info.link_rate * 27))
			mtk_dptx_hal_set_sdp_down_cnt_init_in_hblanking(mtk_dp, 0x0008);
		else
			mtk_dptx_hal_set_sdp_down_cnt_init_in_hblanking(mtk_dp,
									0x0010 + dc_offset);
		break;
	}
}

static void mtk_dptx_set_tu(struct mtk_dp *mtk_dp)
{
	u16 sram_read_start = DPTX_TBC_BUF_READ_START_ADR_THRD;

	if (mtk_dp->training_info.link_lane_count > 0) {
		sram_read_start = mtk_dp->mode->hdisplay /
			(mtk_dp->training_info.link_lane_count * 4 * 2 * 2);
		sram_read_start =
			(sram_read_start < DPTX_TBC_BUF_READ_START_ADR_THRD) ?
			sram_read_start : DPTX_TBC_BUF_READ_START_ADR_THRD;
		mtk_dptx_hal_set_tu_sram_rd_start(mtk_dp, sram_read_start);
	}

	mtk_dptx_hal_set_tu_set_encoder(mtk_dp);
	mtk_dptx_set_sdp_down_cnt_init_in_hblanking(mtk_dp);
	mtk_dptx_set_sdp_down_cnt_init(mtk_dp, sram_read_start);
}

static void mtk_dptx_set_misc(struct mtk_dp *mtk_dp)
{
	u8 format, depth;
	union MISC_T DPTX_MISC;

	DPTX_MISC.dp_misc.is_sync_clock = 0;
	DPTX_MISC.dp_misc.color_format = 0;
	DPTX_MISC.dp_misc.spec_def1 = 0;
	DPTX_MISC.dp_misc.color_depth = 0;
	DPTX_MISC.dp_misc.interlaced = 0;
	DPTX_MISC.dp_misc.stereo_attr = 0;
	DPTX_MISC.dp_misc.reserved = 0;
	DPTX_MISC.dp_misc.is_vsc_sdp = 0;
	DPTX_MISC.dp_misc.spec_def2 = 0;

	format = mtk_dp->info.format;
	depth = mtk_dp->info.depth;

	switch (depth) {
	case DP_COLOR_DEPTH_6BIT:
	case DP_COLOR_DEPTH_8BIT:
	case DP_COLOR_DEPTH_10BIT:
	case DP_COLOR_DEPTH_12BIT:
	case DP_COLOR_DEPTH_16BIT:
	default:
		DPTX_MISC.dp_misc.color_depth = depth;
		break;
	}

	DPTX_MISC.dp_misc.color_format = MTK_DP_COLOR_FORMAT_RGB_444;

	mtk_dptx_hal_set_misc(mtk_dp, DPTX_MISC.misc);
}

static void mtk_dptx_set_dptx_out(struct mtk_dp *mtk_dp)
{
	mtk_dp_msa_bypass_disable(mtk_dp);
	mtk_dptx_set_tu(mtk_dp);
}

static void mtk_dptx_train_update_swing_pre(struct mtk_dp *mtk_dp, int lanes,
					    u8 dpcd_adjust_req[2])
{
	int lane;

	for (lane = 0; lane < lanes; ++lane) {
		u8 val;
		u8 swing;
		u8 preemphasis;
		int index = lane / 2;
		int shift = lane % 2 ? DP_ADJUST_VOLTAGE_SWING_LANE1_SHIFT : 0;

		swing = (dpcd_adjust_req[index] >> shift) &
			DP_ADJUST_VOLTAGE_SWING_LANE0_MASK;
		preemphasis = ((dpcd_adjust_req[index] >> shift) &
			       DP_ADJUST_PRE_EMPHASIS_LANE0_MASK) >>
			      DP_ADJUST_PRE_EMPHASIS_LANE0_SHIFT;
		val = swing << DP_TRAIN_VOLTAGE_SWING_SHIFT |
		      preemphasis << DP_TRAIN_PRE_EMPHASIS_SHIFT;

		if (swing == DP_TRAIN_VOLTAGE_SWING_LEVEL_3)
			val |= DP_TRAIN_MAX_SWING_REACHED;
		if (preemphasis == 3)
			val |= DP_TRAIN_MAX_PRE_EMPHASIS_REACHED;

		mtk_dptx_hal_set_swing_preemphasis(mtk_dp, lane, swing, preemphasis);
		mtk_dptx_aux_write_dpcd(mtk_dp, DP_AUX_NATIVE_WRITE, DP_TRAINING_LANE0_SET + lane,
					DP_AUX_SIZE_BYTE, val);
	}

	mdelay(2);
}

static int mtk_dptx_training_flow(struct mtk_dp *mtk_dp, u8 lane_rate, u8 lane_count)
{
	u8 temp_byte = 0x0;
	u8 link_status[DP_LINK_STATUS_SIZE];
	u8 target_link_rate = lane_rate;
	u8 target_lane_count = lane_count;
	u8 pass_tps1 = false;
	u8 pass_tps2_3 = false;
	u8 train_retry_times;
	u8 status_control;
	u8 iteration_count;
	u8 prev_lane_adjust;
	u8 lane_adjust[2] = {};

	memset(link_status, 0, sizeof(link_status));

	mtk_dptx_aux_read_dpcd(mtk_dp, DP_AUX_NATIVE_READ, DP_SET_POWER, DP_AUX_SIZE_BYTE,
			       &temp_byte);
	if (temp_byte != DP_SET_POWER_D0) {
		temp_byte = DP_SET_POWER_D0;
		mtk_dptx_aux_write_dpcd(mtk_dp, DP_AUX_NATIVE_WRITE, DP_SET_POWER,
					DP_AUX_SIZE_BYTE, &temp_byte);
		mdelay(1);
	}

	link_status[0] = target_link_rate;
	link_status[1] = target_lane_count | DPTX_AUX_SET_ENAHNCED_FRAME;
	mtk_dptx_aux_write_dpcd(mtk_dp, DP_AUX_NATIVE_WRITE, DP_LINK_BW_SET, DP_AUX_SIZE_2_BYTE,
				link_status);

	if (mtk_dp->training_info.sink_ssc_en) {
		temp_byte = DP_SPREAD_AMP_0_5;
		mtk_dptx_aux_write_dpcd(mtk_dp, DP_AUX_NATIVE_WRITE, DP_DOWNSPREAD_CTRL,
					DP_AUX_SIZE_BYTE, &temp_byte);
	}

	train_retry_times = 0;
	status_control = 0;
	iteration_count = 1;
	prev_lane_adjust = 0xFF;

	mtk_dptx_hal_set_tx_lane(mtk_dp, target_lane_count / 2);
	mtk_dptx_hal_set_tx_rate(mtk_dp, target_link_rate);

	do {
		train_retry_times++;

		if (!pass_tps1)	{
			dptx_printf("%s CR training start\n", __func__);
			mtk_dptx_hal_set_scramble(mtk_dp, false);

			if (status_control == 0)	{
				mtk_dptx_hal_set_tx_training_pattern(mtk_dp, TPS_1_pattern);
				status_control = 1;
				temp_byte = DP_LINK_SCRAMBLING_DISABLE |
				      DP_TRAINING_PATTERN_1;
				mtk_dptx_aux_write_dpcd(mtk_dp, DP_AUX_NATIVE_WRITE,
							DP_TRAINING_PATTERN_SET, DP_AUX_SIZE_BYTE,
							&temp_byte);
				mtk_dptx_aux_read_dpcd(mtk_dp, DP_AUX_NATIVE_READ,
						       DP_ADJUST_REQUEST_LANE0_1,
						       sizeof(lane_adjust), lane_adjust);
				iteration_count++;
				mtk_dptx_train_update_swing_pre(mtk_dp, target_lane_count,
								lane_adjust);
			}

			drm_dp_link_train_clock_recovery_delay(mtk_dp->rx_cap);
			mtk_dptx_aux_read_dpcd(mtk_dp, DP_AUX_NATIVE_READ, DP_LANE0_1_STATUS,
					       DP_LINK_STATUS_SIZE, link_status);

			if (drm_dp_clock_recovery_ok(link_status, target_lane_count)) {
				dptx_printf("CR Training Success\n");
				mtk_dp->training_info.cr_done = true;
				pass_tps1 = true;
				train_retry_times = 0;
				iteration_count = 1;
			} else {
				if (prev_lane_adjust == link_status[4]) {
					iteration_count++;
					if (prev_lane_adjust & DP_ADJUST_VOLTAGE_SWING_LANE0_MASK)
						iteration_count = DPTX_TRAIN_MAX_ITERATION;
				} else {
					prev_lane_adjust = link_status[4];
				}

				dptx_printf("CR Training Fail\n");
			}
		} else if (pass_tps1 && !pass_tps2_3) {
			dptx_printf("%s EQ training start\n", __func__);
			if (status_control == 1) {
				if (mtk_dp->training_info.tps4) {
					mtk_dptx_hal_set_tx_training_pattern(mtk_dp,
									     TPS_4_pattern);
					dptx_printf("LT TPS4\n");
				} else if (mtk_dp->training_info.tps3) {
					mtk_dptx_hal_set_tx_training_pattern(mtk_dp,
									     TPS_3_pattern);
					dptx_printf("LT TP3\n");
				} else {
					mtk_dptx_hal_set_tx_training_pattern(mtk_dp,
									     TPS_2_pattern);
					dptx_printf("LT TPS2\n");
				}

				if (mtk_dp->training_info.tps4) {
					temp_byte = DP_TRAINING_PATTERN_4;
					mtk_dptx_aux_write_dpcd(mtk_dp, DP_AUX_NATIVE_WRITE,
								DP_TRAINING_PATTERN_SET,
								DP_AUX_SIZE_BYTE, &temp_byte);

				} else if (mtk_dp->training_info.tps3) {
					temp_byte = DP_LINK_SCRAMBLING_DISABLE |
					      DP_TRAINING_PATTERN_3;
					mtk_dptx_aux_write_dpcd(mtk_dp, DP_AUX_NATIVE_WRITE,
								DP_TRAINING_PATTERN_SET,
								DP_AUX_SIZE_BYTE, &temp_byte);
				} else {
					temp_byte = DP_LINK_SCRAMBLING_DISABLE |
					      DP_TRAINING_PATTERN_2;
					mtk_dptx_aux_write_dpcd(mtk_dp, DP_AUX_NATIVE_WRITE,
								DP_TRAINING_PATTERN_SET,
								DP_AUX_SIZE_BYTE, &temp_byte);
				}

				status_control = 2;
				mtk_dptx_aux_read_dpcd(mtk_dp, DP_AUX_NATIVE_READ,
						       DP_ADJUST_REQUEST_LANE0_1,
						       sizeof(lane_adjust), lane_adjust);

				iteration_count++;
				mtk_dptx_train_update_swing_pre(mtk_dp, target_lane_count,
								lane_adjust);
			}
			drm_dp_link_train_channel_eq_delay(mtk_dp->rx_cap);

			mtk_dptx_aux_read_dpcd(mtk_dp, DP_AUX_NATIVE_READ, DP_LANE0_1_STATUS,
					       DP_LINK_STATUS_SIZE, link_status);

			if (!drm_dp_clock_recovery_ok(link_status, target_lane_count)) {
				mtk_dp->training_info.cr_done = false;
				mtk_dp->training_info.eq_done = false;
				break;
			}

			if (drm_dp_channel_eq_ok(link_status, target_lane_count)) {
				mtk_dp->training_info.eq_done = true;
				pass_tps2_3 = true;
				dptx_printf("EQ Training Success\n");
				break;
			}
			dptx_printf("EQ Training Fail\n");

			if (prev_lane_adjust == link_status[4])
				iteration_count++;
			else
				prev_lane_adjust = link_status[4];
		}

		mtk_dptx_train_update_swing_pre(mtk_dp, target_lane_count, lane_adjust);
		dptx_printf("train_retry_times = %d, iteration_count = %d\n", train_retry_times,
			    iteration_count);

	} while ((train_retry_times < DPTX_TRAIN_RETRY_LIMIT) &&
		(iteration_count < DPTX_TRAIN_MAX_ITERATION));

	temp_byte = DP_TRAINING_PATTERN_DISABLE;
	mtk_dptx_aux_write_dpcd(mtk_dp, DP_AUX_NATIVE_WRITE, DP_TRAINING_PATTERN_SET,
				DP_AUX_SIZE_BYTE, &temp_byte);
	mtk_dptx_hal_set_tx_training_pattern(mtk_dp, 0);

	if (pass_tps2_3) {
		mtk_dp->training_info.link_rate = target_link_rate;
		mtk_dp->training_info.link_lane_count = target_lane_count;

		mtk_dptx_hal_set_scramble(mtk_dp, true);

		temp_byte = target_lane_count
			| DPTX_AUX_SET_ENAHNCED_FRAME;

		mtk_dptx_aux_write_dpcd(mtk_dp, DP_AUX_NATIVE_WRITE, DP_LANE_COUNT_SET,
					DP_AUX_SIZE_BYTE, &temp_byte);
		mtk_dptx_hal_set_ef_mode(mtk_dp, ENABLE_DPTX_EF_MODE);

		dptx_printf("Link Training PASS\n");
		return DPTX_NOERR;
	}

	printf("Link Training Fail\n");
	return DPTX_TRANING_FAIL;
}

static bool mtk_dptx_check_sink_cap(struct mtk_dp *mtk_dp)
{
	u8 temp_byte;
	u8 temp_buffer[DP_AUX_MAX_PAYLOAD_BYTES];

	memset(temp_buffer, 0, sizeof(temp_buffer));

	temp_byte = DP_SET_POWER_D0;
	mtk_dptx_aux_write_dpcd(mtk_dp, DP_AUX_NATIVE_WRITE, DP_SET_POWER, DP_AUX_SIZE_BYTE,
				&temp_byte);
	mdelay(2);

	mtk_dptx_aux_read_dpcd(mtk_dp, DP_AUX_NATIVE_READ, DP_DPCD_REV, DP_AUX_MAX_PAYLOAD_BYTES,
			       temp_buffer);

	mtk_dp->training_info.sink_extcap_en =
		(temp_buffer[DP_TRAINING_AUX_RD_INTERVAL] & DP_TRAINING_AUX_RD_MASK) ?
		true : false;

	if (mtk_dp->training_info.sink_extcap_en)
		mtk_dptx_aux_read_dpcd(mtk_dp, DP_AUX_NATIVE_READ, DP_DP13_DPCD_REV,
				       DP_AUX_MAX_PAYLOAD_BYTES, temp_buffer);

	mtk_dp->training_info.dpcd_dev = temp_buffer[DP_DPCD_REV];
	dptx_printf("SINK DPCD version:0x%x\n", mtk_dp->training_info.dpcd_dev);

	memcpy(mtk_dp->rx_cap, temp_buffer, sizeof(temp_buffer));
	mtk_dp->rx_cap[DP_TRAINING_AUX_RD_INTERVAL] &= DP_TRAINING_AUX_RD_MASK;

	mtk_dp->training_info.link_rate =
		(temp_buffer[DP_MAX_LINK_RATE] >= mtk_dp->training_info.sys_max_link_rate) ?
		mtk_dp->training_info.sys_max_link_rate : temp_buffer[DP_MAX_LINK_RATE];
	mtk_dp->training_info.link_lane_count =
		((temp_buffer[DP_MAX_LANE_COUNT] & DP_MAX_LANE_COUNT_MASK) >= MAX_LANECOUNT) ?
		MAX_LANECOUNT : (temp_buffer[DP_MAX_LANE_COUNT] & DP_MAX_LANE_COUNT_MASK);

	mtk_dp->training_info.tps3 =
		(temp_buffer[DP_MAX_LANE_COUNT] & DP_TPS3_SUPPORTED) >> DP_TPS3_SHIFT;
	mtk_dp->training_info.tps4 =
		(temp_buffer[DP_MAX_DOWNSPREAD] & DP_TPS4_SUPPORTED) >> DP_TPS4_SHIFT;

	mtk_dp->training_info.dwn_strm_port_present =
		(temp_buffer[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_PRESENT);

	if (temp_buffer[DP_MAX_DOWNSPREAD] & DP_MAX_DOWNSPREAD_0_5) {
		mtk_dp->training_info.sink_ssc_en = true;
		dptx_printf("SINK SUPPORT SSC!\n");
	} else {
		mtk_dp->training_info.sink_ssc_en = false;
		dptx_printf("SINK NOT SUPPORT SSC!\n");
	}

	mtk_dptx_aux_read_dpcd(mtk_dp, DP_AUX_NATIVE_READ, DP_MSTM_CAP, DP_AUX_SIZE_BYTE,
			       &temp_byte);
	mtk_dp->training_info.dp_mst_cap = (temp_byte & DP_MST_CAP);
	mtk_dp->training_info.dp_mst_branch = false;

	if (mtk_dp->training_info.dp_mst_cap == DP_MST_CAP) {
		if (mtk_dp->training_info.dwn_strm_port_present)
			mtk_dp->training_info.dp_mst_branch = true;

		mtk_dptx_aux_read_dpcd(mtk_dp, DP_AUX_NATIVE_READ,
				       DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0, DP_AUX_SIZE_BYTE,
				       &temp_byte);
		if (temp_byte)
			mtk_dptx_aux_write_dpcd(mtk_dp, DP_AUX_NATIVE_WRITE,
						DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0,
						DP_AUX_SIZE_BYTE, &temp_byte);
	}

	return true;
}

static bool mtk_dptx_training_change_mode(struct mtk_dp *mtk_dp)
{
	mtk_dptx_hal_phyd_reset(mtk_dp);
	mtk_dptx_hal_reset_swing_preemphasis(mtk_dp);
	mtk_dptx_hal_ssc_on_off_setting(mtk_dp, mtk_dp->training_info.sink_ssc_en);

	mdelay(2);
	return true;
}

static int mtk_dptx_set_training_start(struct mtk_dp *mtk_dp)
{
	u8 ret = DPTX_NOERR;
	u8 lane_count;
	u8 link_rate;
	u8 temp_byte;
	u8 train_time_limits;
	u8 max_link_rate;

	temp_byte = DP_SET_POWER_D0;
	mtk_dptx_aux_write_dpcd(mtk_dp, DP_AUX_NATIVE_WRITE, DP_SET_POWER, DP_AUX_SIZE_BYTE,
				&temp_byte);

	link_rate = mtk_dp->rx_cap[DP_MAX_LINK_RATE];
	lane_count = mtk_dp->rx_cap[DP_MAX_LANE_COUNT] & DP_MAX_LANE_COUNT_MASK;
	dptx_printf("RX support link_rate = 0x%x,lane_count = %x", link_rate, lane_count);

	mtk_dp->training_info.link_rate =
		(link_rate >= mtk_dp->training_info.sys_max_link_rate) ?
		mtk_dp->training_info.sys_max_link_rate : link_rate;
	mtk_dp->training_info.link_lane_count =
		(lane_count >= MAX_LANECOUNT) ?
		MAX_LANECOUNT : lane_count;

	if (mtk_dp->training_info.sink_extcap_en)
		mtk_dptx_aux_read_dpcd(mtk_dp, DP_AUX_NATIVE_READ, DP_SINK_COUNT_ESI,
				       DP_AUX_SIZE_BYTE, &temp_byte);
	else
		mtk_dptx_aux_read_dpcd(mtk_dp, DP_AUX_NATIVE_READ, DP_SINK_COUNT,
				       DP_AUX_SIZE_BYTE, &temp_byte);

	if (temp_byte & DP_SINK_COUNT_MASK) {
		mtk_dp->training_info.sink_count_num = temp_byte & DP_SINK_COUNT_MASK;
		dptx_printf("ExtSink Count = %d\n", mtk_dp->training_info.sink_count_num);
	}

	link_rate = mtk_dp->training_info.link_rate;
	lane_count = mtk_dp->training_info.link_lane_count;

	switch (link_rate) {
	case DP_LINKRATE_RBR:
	case DP_LINKRATE_HBR:
	case DP_LINKRATE_HBR2:
	case DP_LINKRATE_HBR25:
	case DP_LINKRATE_HBR3:
		break;
	default:
		mtk_dp->training_info.link_rate = DP_LINKRATE_HBR3;
		break;
	};

	max_link_rate = link_rate;
	train_time_limits = DPTX_TRAIN_RETRY_LIMIT;

	do {
		dptx_printf("LinkRate:0x%x, LaneCount:%x", link_rate, lane_count);

		mtk_dp->training_info.cr_done = false;
		mtk_dp->training_info.eq_done = false;

		mtk_dptx_training_change_mode(mtk_dp);
		ret = mtk_dptx_training_flow(mtk_dp, link_rate, lane_count);
		if (ret == DPTX_PLUG_OUT || ret == DPTX_TRANING_STATE_CHANGE)
			return ret;

		if (!mtk_dp->training_info.cr_done) {
			switch (link_rate) {
			case DP_LINKRATE_RBR:
				lane_count = lane_count / 2;
				link_rate = max_link_rate;
				if (lane_count == 0x0)
					return DPTX_TRANING_FAIL;
				break;
			case DP_LINKRATE_HBR:
				link_rate = DP_LINKRATE_RBR;
				break;
			case DP_LINKRATE_HBR2:
				link_rate = DP_LINKRATE_HBR;
				break;
			case DP_LINKRATE_HBR3:
				link_rate = DP_LINKRATE_HBR2;
				break;
			default:
				return DPTX_TRANING_FAIL;
			}
			train_time_limits--;
		} else if (!mtk_dp->training_info.eq_done) {
			if (lane_count == DP_LANECOUNT_4)
				lane_count = DP_LANECOUNT_2;
			else if (lane_count == DP_LANECOUNT_2)
				lane_count = DP_LANECOUNT_1;
			else
				return DPTX_TRANING_FAIL;
			train_time_limits--;
		} else {
			dptx_printf("training info cr done\n");
			return DPTX_NOERR;
		}

	} while (train_time_limits > 0);

	return DPTX_TRANING_FAIL;
}

static void mtk_dptx_init_port(struct mtk_dp *mtk_dp)
{
	mtk_dptx_hal_pht_set_idle_pattern(mtk_dp, true);
	mtk_dptx_hal_initial_setting(mtk_dp);
	mtk_dptx_hal_aux_setting(mtk_dp);
	mtk_dptx_hal_digital_setting(mtk_dp);
	mtk_dptx_hal_phy_setting(mtk_dp);
	mtk_dptx_hal_hpd_detect_setting(mtk_dp);

	mtk_dptx_hal_digital_sw_reset(mtk_dp);
	mtk_dptx_hal_analog_power_on_off(mtk_dp, true);
	mtk_dptx_video_mute(mtk_dp, true);
}

static void mtk_dptx_video_enable(struct mtk_dp *mtk_dp, bool enable)
{
	if (enable) {
		mtk_dptx_set_dptx_out(mtk_dp);
		mtk_dptx_video_mute(mtk_dp, false);
	} else {
		dptx_printf("%s mute\n", __func__);
		mtk_dptx_video_mute(mtk_dp, true);
	}
}

static void mtk_dptx_set_color_format(struct mtk_dp *mtk_dp, u8 color_format)
{
	mtk_dptx_hal_set_color_format(mtk_dp, color_format);
}

static void mtk_dptx_set_color_depth(struct mtk_dp *mtk_dp, u8 color_depth)
{
	mtk_dptx_hal_set_color_depth(mtk_dp, color_depth);
}

static void mtk_dp_video_config(struct mtk_dp *mtk_dp)
{
	u32 mvid = 0;
	bool overwrite = false;

	if (mtk_dp->has_dsc) {
		u8 data[1];

		data[0] = (u8)mtk_dp->dsc_enable;
		mtk_dptx_aux_write_dpcd(mtk_dp, DP_AUX_NATIVE_WRITE, 0x160, 0x1, data);
	}

	mtk_dptx_hal_set_msa(mtk_dp);

	mtk_dptx_set_misc(mtk_dp);

	mtk_dptx_set_color_depth(mtk_dp, mtk_dp->info.depth);
	mtk_dptx_set_color_format(mtk_dp, mtk_dp->info.format);
}

struct display_mode *mtk_dp_checkedid(void)
{
	unsigned short h_active, v_active, h_blanking, v_blanking, hsync, vsync;
	unsigned short hfp, vfp, hbp, vbp, fps;
	struct edp_panel_description *edp_panel_desc = NULL;
	struct display_mode *mode;

	edp_panel_get_desc(&edp_panel_desc);
	if (edp_panel_desc)
		mode = edp_panel_desc->mode;
	else
		return NULL;

	h_active = (unsigned short)(*(edid_data  + 0x3A) & 0xF0) << 4;
	h_active |= *(edid_data  + 0x38);
	h_blanking = (unsigned short)(*(edid_data + 0x3A) & 0x0F) << 8;
	h_blanking |= *(edid_data + 0x39);

	v_active = (unsigned short)(*(edid_data  + 0x3D) & 0xF0) << 4;
	v_active |= *(edid_data  + 0x3B);
	v_blanking = (unsigned short)(*(edid_data + 0x3D) & 0x0F) << 8;
	v_blanking |= (unsigned short)(*(edid_data + 0x3C));

	hsync = (unsigned short)(*(edid_data + 0x3F));
	hsync |= (unsigned short)(*(edid_data + 0x41) & 0x30) << 4;
	hfp = (unsigned short)(*(edid_data + 0x3E));
	hfp |= (unsigned short)(*(edid_data + 0x41) & 0xC0) << 2;

	vsync = (unsigned short)(*(edid_data + 0x40) & 0x0F);
	vsync |= (unsigned short)(*(edid_data + 0x41) & 0x03) << 8;
	vfp = (unsigned short)(*(edid_data + 0x40) & 0xF0) >> 4;
	vfp |= (unsigned short)(*(edid_data + 0x41) & 0x0C) << 6;
	hbp = h_blanking - hfp - hsync;
	vbp = v_blanking - vfp - vsync;
	fps = mode->clock * 1000 / (h_active + h_blanking) / (v_active + v_blanking);

	dptx_printf("pixel_clock   = %d\n", mode->clock);
	dptx_printf("h_active    = %d\n", h_active);
	dptx_printf("h_blanking  = %d\n", h_blanking);
	dptx_printf("hsync         = %d\n", hsync);
	dptx_printf("hfp           = %d\n", hfp);
	dptx_printf("hbp           = %d\n", hbp);
	dptx_printf("v_active    = %d\n", v_active);
	dptx_printf("v_blanking  = %d\n", v_blanking);
	dptx_printf("vsync         = %d\n", vsync);
	dptx_printf("vfp           = %d\n", vfp);
	dptx_printf("vbp           = %d\n", vbp);
	dptx_printf("fps           = %d\n", fps);

	mode->hdisplay = h_active;
	mode->hsync_start = h_active + hfp;
	mode->hsync_end = h_active + hfp + hsync;
	mode->htotal = h_active + hfp + hsync + hbp;
	mode->vdisplay = v_active;
	mode->vsync_start = v_active + vfp;
	mode->vsync_end = v_active + vfp + vsync;
	mode->vtotal = v_active + vfp + vsync + vbp;

	return mode;
}

struct display_mode *mtk_dp_force_get_modes(void)
{
	printf("%s provide a set of default mode\n", __func__);

	g_mode.clock = 65000;
	g_mode.hdisplay = 1024;
	g_mode.hsync_start = 1024 + 24;
	g_mode.hsync_end = 1024 + 24 + 136;
	g_mode.htotal = 1024 + 24 + 136 + 160;
	g_mode.vdisplay = 768;
	g_mode.vsync_start = 768 + 3;
	g_mode.vsync_end = 768 + 3 + 6;
	g_mode.vtotal = 768 + 3 + 6 + 29;

	return &g_mode;
}

static int mtk_edp_probe(struct udevice *dev)
{
	struct mtk_dp mtk_edp;
	struct mtk_dpintf_priv *dpintf = dev_get_priv(dev);
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);
	struct video_priv *priv = dev_get_uclass_priv(dev);
	struct edp_panel_description *edp_panel_desc = NULL;
	int ret;

	dpintf->dev = dev;

	edp_panel_get_desc(&edp_panel_desc);
	ret = gpio_request_by_name(dev, "vcc-gpios", 0, &edp_panel_desc->vcc_gpio, GPIOD_IS_OUT);
	if (ret) {
		printf("Warning: %s cannot get vcc GPIO: ret=%d\n", ret);
		return ret;
	}

	ret = gpio_request_by_name(dev, "pwm-gpios", 0, &edp_panel_desc->pwm_gpio, GPIOD_IS_OUT);
	if (ret) {
		printf("Warning: cannot get pwm GPIO: ret=%d\n", ret);
		return ret;
	}

	ret = gpio_request_by_name(dev, "bl-en-gpios", 0, &edp_panel_desc->bl_en_gpio,
				   GPIOD_IS_OUT);
	if (ret) {
		printf("Warning: cannot get bl-en GPIO: ret=%d\n", ret);
		return ret;
	}

	ret = gpio_request_by_name(dev, "bl-vcc-gpios", 0, &edp_panel_desc->bl_vcc_gpio,
				   GPIOD_IS_OUT);
	if (ret)
		printf("Warning: cannot get bl-vcc GPIO: ret=%d\n", ret);

	if (edp_panel_desc->power_on)
		edp_panel_desc->power_on();

	mdelay(100);

	mtk_dptx_init_variable(&mtk_edp);
	mtk_dptx_init_port(&mtk_edp);

	mdelay(20);

	if (!mtk_dptx_hal_hpd_high(&mtk_edp)) {
		printf("[edpLK] eDPTx HPD is low, no device connected\n");
		mtk_edp.mode = mtk_dp_force_get_modes();
		dp_intf_config(mtk_edp.mode, dpintf);
		return -1;
	}

	mtk_dptx_check_sink_cap(&mtk_edp);
	mtk_dp_get_edid(&mtk_edp);

	mtk_edp.mode = mtk_dp_checkedid();

	priv->bpix = VIDEO_BPP32;
	priv->xsize = mtk_edp.mode->hdisplay;
	priv->ysize = mtk_edp.mode->vdisplay;
	priv->line_length = mtk_edp.mode->hdisplay * VNBYTES(VIDEO_BPP32);

	mtk_maindisp_set_w_h(priv->xsize, priv->ysize);
	mt_maindisp_set_fb_addr(plat->base);

	mt_maindisp_set_scenario_edp();
	mtk_maindisp_update(0, 0, priv->xsize, priv->ysize);

	video_set_flush_dcache(dev, true);

	mtk_dptx_set_training_start(&mtk_edp);
	mtk_dp_video_config(&mtk_edp);
	dp_intf_config(mtk_edp.mode, dpintf);
	mtk_dptx_video_enable(&mtk_edp, true);

	return 0;
}

static int mtk_edp_bind(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);

	plat->size = MTK_DP_MAX_WIDTH * MTK_DP_MAX_HEIGHT * VNBYTES(VIDEO_BPP32);

	plat->base = dev_read_addr_ptr(dev);
	if (!plat->base) {
		printf("%s: No framebuffer address\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static const struct udevice_id mtk_dptx_hal_ids[] = {
	{ .compatible = "mediatek,mt8195-edp-tx" },
	{ .compatible = "mediatek,mt8188-edp-tx" },
	{}
};

static int mtk_edp_remove(struct udevice *dev)
{
	struct mtk_dpintf_priv *dpintf = dev_get_priv(dev);

	mtk_maindisp_reset();

	return 0;
}

U_BOOT_DRIVER(mtk_dp) = {
	.name	   = "mtk_dp",
	.id	   = UCLASS_VIDEO,
	.of_match  = mtk_dptx_hal_ids,
	.bind	   = mtk_edp_bind,
	.probe	   = mtk_edp_probe,
	.priv_auto = sizeof(struct mtk_dp),
	.remove    = mtk_edp_remove,
	.flags	   = DM_FLAG_PRE_RELOC | DM_FLAG_LEAVE_PD_ON | DM_FLAG_OS_PREPARE,
};
