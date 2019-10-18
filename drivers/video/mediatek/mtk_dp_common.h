/* SPDX-License-Identifier: GPL-2.0
 *
 * MediaTek dp common header
 *
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Tommy Chen <tommyyl.chen@mediatek.com>
 */

#ifndef __DP_COMMON_H__
#define __DP_COMMON_H__

#include <string.h>

#define EDID_SIZE 0x200
#define ENABLE_DPTX_SSC_FORCEON			0x0
#define ENABLE_DPTX_FIX_LRLC			0x0
#define ENABLE_DPTX_SSC_OUTPUT			0x1
#define ENABLE_DPTX_FIX_TPS2			0x0
#define AUX_WRITE_READ_WAIT_TIME		0x14
#define DPTX_SUPPORT_DSC			0x0

#define DPTX_TBC_SELBUF_CASE			0x2
#define DPTX_TBC_BUF_SIZE			DPTX_TBC_SELBUF_CASE
#if (DPTX_TBC_SELBUF_CASE == 2)
#define DPTX_TBC_BUF_READ_START_ADR_THRD	0x08
#elif (DPTX_TBC_SELBUF_CASE == 1)
#define DPTX_TBC_BUF_READ_START_ADR_THRD	0x10
#else
#define DPTX_TBC_BUF_READ_START_ADR_THRD	0x1f
#endif

#define ENABLE_DPTX_EF_MODE			0x1
#if (ENABLE_DPTX_EF_MODE == 0x01)
#define DPTX_AUX_SET_ENAHNCED_FRAME		0x80
#else
#define DPTX_AUX_SET_ENAHNCED_FRAME		0x00
#endif

#define ONE_BLOCK_SIZE				128
#define TWO_BLOCK_SIZE				256

#define MTK_DP_MAX_WIDTH			3840
#define MTK_DP_MAX_HEIGHT			2160

#define DP_LINK_CONSTANT_N_VALUE		0x8000
#define DP_LINK_STATUS_SIZE			0x6

#define DP_LANE0_1_STATUS			0x202
#define DP_LANE2_3_STATUS			0x203
#define DP_LANE_CR_DONE				BIT(0)
#define DP_LANE_CHANNEL_EQ_DONE			BIT(1)
#define DP_LANE_SYMBOL_LOCKED			BIT(2)

#define DP_BRANCH_OUI_HEADER_SIZE		0xc
#define DP_RECEIVER_CAP_SIZE			0xf
#define DP_DSC_RECEIVER_CAP_SIZE		0xf
#define EDP_PSR_RECEIVER_CAP_SIZE		0x2
#define EDP_DISPLAY_CTL_CAP_SIZE		0x3
#define DP_LTTPR_COMMON_CAP_SIZE		0x8
#define DP_LTTPR_PHY_CAP_SIZE			0x3

#define DP_AUX_SIZE_BYTE			0x1
#define DP_AUX_SIZE_2_BYTE			0x2

#define DP_SET_POWER				0x600
#define DP_SET_POWER_D0				0x1
#define DP_SET_POWER_D3				0x2
#define DP_SET_POWER_MASK			0x3
#define DP_SET_POWER_D3_AUX_ON			0x5

#define DP_EDP_DPCD_REV				0x700
#define DP_EDP_11				0x0
#define DP_EDP_12				0x1
#define DP_EDP_13				0x2
#define DP_EDP_14				0x3
#define DP_EDP_14a				0x4
#define DP_EDP_14b				0x5

#define DP_DPCD_REV				0x0
#define DP_DPCD_REV_10				0x10
#define DP_DPCD_REV_11				0x11
#define DP_DPCD_REV_12				0x12
#define DP_DPCD_REV_13				0x13
#define DP_DPCD_REV_14				0x14

#define DP_MAX_LINK_RATE			0x1
#define DP_MAX_LANE_COUNT			0x2
#define DP_MAX_LANE_COUNT_MASK			0x1f
#define DP_TPS3_SUPPORTED			BIT(6)
#define DP_TPS3_SHIFT				0x6
#define DP_ENHANCED_FRAME_CAP			BIT(7)

#define DP_MAX_DOWNSPREAD			0x3
#define DP_MAX_DOWNSPREAD_0_5			BIT(0)
#define DP_STREAM_REGENERATION_STATUS_CAP	BIT(1)
#define DP_NO_AUX_HANDSHAKE_LINK_TRAINING	BIT(6)
#define DP_TPS4_SUPPORTED			BIT(7)
#define DP_TPS4_SHIFT				0x7

#define DP_DOWNSTREAMPORT_PRESENT		0x5
#define DP_DWN_STRM_PORT_PRESENT		BIT(0)

#define DP_TRAINING_AUX_RD_INTERVAL		0xe
#define DP_TRAINING_AUX_RD_MASK			0x7f
#define DP_EXTENDED_RECEIVER_CAP_FIELD_PRESENT	BIT(7)

#define DP_MSTM_CAP				0x21
#define DP_MST_CAP				BIT(0)
#define DP_SINGLE_STREAM_SIDEBAND_MSG		BIT(1)

/* Link Configuration */
#define	DP_LINK_BW_SET				0x100
#define DP_LINK_RATE_TABLE			0x00
#define DP_LINK_BW_1_62				0x06
#define DP_LINK_BW_2_7				0x0a
#define DP_LINK_BW_5_4				0x14
#define DP_LINK_BW_8_1				0x1e
#define DP_LINK_BW_10				0x01
#define DP_LINK_BW_13_5				0x04
#define DP_LINK_BW_20				0x02

#define DP_LANE_COUNT_SET			0x101
#define DP_LANE_COUNT_MASK			0x0f
#define DP_LANE_COUNT_ENHANCED_FRAME_EN		BIT(7)

#define DP_TRAINING_PATTERN_SET			0x102
#define DP_TRAINING_PATTERN_DISABLE		0x0
#define DP_TRAINING_PATTERN_1			0x1
#define DP_TRAINING_PATTERN_2			0x2
#define DP_TRAINING_PATTERN_2_CDS		0x3
#define DP_TRAINING_PATTERN_3			0x3
#define DP_TRAINING_PATTERN_4			0x7
#define DP_TRAINING_PATTERN_MASK		0x3
#define DP_TRAINING_PATTERN_MASK_1_4		0xf
#define DP_LINK_SCRAMBLING_DISABLE		BIT(5)

#define DP_TRAINING_LANE0_SET			0x103

#define DP_TRAIN_VOLTAGE_SWING_MASK		0x3
#define DP_TRAIN_VOLTAGE_SWING_SHIFT		0x0
#define DP_TRAIN_MAX_SWING_REACHED		BIT(2)
#define DP_TRAIN_VOLTAGE_SWING_LEVEL_0		0x0
#define DP_TRAIN_VOLTAGE_SWING_LEVEL_1		0x1
#define DP_TRAIN_VOLTAGE_SWING_LEVEL_2		0x2
#define DP_TRAIN_VOLTAGE_SWING_LEVEL_3		0x3

#define DP_TRAIN_PRE_EMPHASIS_SHIFT		0x3
#define DP_TRAIN_MAX_PRE_EMPHASIS_REACHED	BIT(5)

#define DP_DOWNSPREAD_CTRL			0x107
#define DP_SPREAD_AMP_0_5			BIT(4)
#define DP_MSA_TIMING_PAR_IGNORE_EN		BIT(7)

#define DP_SINK_COUNT				0x200
#define DP_SINK_CP_READY			BIT(6)
#define DP_SINK_COUNT_MASK			0xBF

#define DP_CHANNEL_EQ_BITS			(DP_LANE_CR_DONE |\
						 DP_LANE_CHANNEL_EQ_DONE |\
						 DP_LANE_SYMBOL_LOCKED)

#define DP_LANE_ALIGN_STATUS_UPDATED		0x204
#define DP_INTERLANE_ALIGN_DONE			BIT(0)
#define DP_DOWNSTREAM_PORT_STATUS_CHANGED	BIT(6)
#define DP_LINK_STATUS_UPDATED			BIT(7)

#define DP_SINK_STATUS				0x205
#define DP_RECEIVE_PORT_0_STATUS		BIT(0)
#define DP_RECEIVE_PORT_1_STATUS		BIT(1)
#define DP_STREAM_REGENERATION_STATUS		BIT(2)

#define DP_ADJUST_REQUEST_LANE0_1		0x206
#define DP_ADJUST_REQUEST_LANE2_3		0x207
#define DP_ADJUST_VOLTAGE_SWING_LANE0_MASK	0x03
#define DP_ADJUST_VOLTAGE_SWING_LANE0_SHIFT	0x0
#define DP_ADJUST_PRE_EMPHASIS_LANE0_MASK	0xc
#define DP_ADJUST_PRE_EMPHASIS_LANE0_SHIFT	0x2
#define DP_ADJUST_VOLTAGE_SWING_LANE1_MASK	0x30
#define DP_ADJUST_VOLTAGE_SWING_LANE1_SHIFT	0x4
#define DP_ADJUST_PRE_EMPHASIS_LANE1_MASK	0xc0
#define DP_ADJUST_PRE_EMPHASIS_LANE1_SHIFT	0x6

#define DP_SINK_COUNT_ESI			0x2002
#define DP_SINK_COUNT_CP_READY			BIT(6)

#define DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0	0x2003

#define DP_LANE0_1_STATUS_ESI			0x200c

#define DP_DP13_DPCD_REV			0x2200

#define DP_AUX_MAX_PAYLOAD_BYTES		0x10

#define MAX_LANECOUNT				0x4

#define EDID_ADDR_EXT_BLOCK_FLAG		0x7e

#define DPTX_TRAIN_RETRY_LIMIT			0x8
#define DPTX_TRAIN_MAX_ITERATION		0x5

#define HPD_INT_EVNET				BIT(3)
#define HPD_CONNECT				BIT(2)
#define HPD_DISCONNECT				BIT(1)
#define HPD_INITIAL_STATE			0x0

#define MTK_DP_AUX_WAIT_REPLY_COUNT		0x14

union MISC_T {
	struct {
		u8 is_sync_clock : 1;
		u8 color_format : 2;
		u8 spec_def1 : 2;
		u8 color_depth : 3;
		u8 interlaced : 1;
		u8 stereo_attr : 2;
		u8 reserved : 3;
		u8 is_vsc_sdp : 1;
		u8 spec_def2 : 1;
	} dp_misc;
	u8 misc[2];
};

struct DPTX_TRAINING_INFO {
	bool sink_extcap_en;
	bool tps3;
	bool tps4;
	bool sink_ssc_en;
	bool dp_mst_cap;
	bool dp_mst_branch;
	bool dwn_strm_port_present;
	bool cr_done;
	bool eq_done;

	u8 sys_max_link_rate;
	u8 link_rate;
	u8 link_lane_count;
	u8 dpcd_dev;
	u8 sink_count_num;
};

struct DPTX_INFO {
	u8 depth;
	u8 format;
	u8 resolution;
};

struct mtk_dp_driver_data {
	bool is_edp;
};

struct mtk_dp {
	int id;
	u8 rx_cap[16];
	struct DPTX_INFO info;
	int state;
	int state_pre;
	struct DPTX_TRAINING_INFO training_info;
	int training_state;
	struct display_mode *mode;

	u8 irq_status;
	u32 min_clock;
	u32 max_clock;
	u32 max_hdisplay;
	u32 max_vdisplay;

	void *regs;

	int disp_status;
	bool power_on;
	bool audio_enable;
	bool video_enable;
	bool dp_ready;
	bool has_dsc;
	bool has_fec;
	bool dsc_enable;
	bool enabled;
	bool powered;
};

enum DP_ATF_CMD {
	DP_ATF_DUMP = 0x20,
	DP_ATF_VIDEO_UNMUTE = 0x20,
	DP_ATF_EDP_VIDEO_UNMUTE,
	DP_ATF_REG_WRITE,
	DP_ATF_REG_READ,
	DP_ATF_CMD_COUNT
};

enum DP_LANECOUNT {
	DP_LANECOUNT_1 = 0x1,
	DP_LANECOUNT_2 = 0x2,
	DP_LANECOUNT_4 = 0x4,
};

enum DP_VERSION {
	DP_VERSION_11 = 0x11,
	DP_VERSION_12 = 0x12,
	DP_VERSION_14 = 0x14,
	DP_VERSION_12_14 = 0x16,
	DP_VERSION_14_14 = 0x17,
	DP_VERSION_MAX,
};

enum DP_LINKRATE {
	DP_LINKRATE_RBR = 0x6,
	DP_LINKRATE_HBR = 0xA,
	DP_LINKRATE_HBR2 = 0x14,
	DP_LINKRATE_HBR25 = 0x19,
	DP_LINKRATE_HBR3 = 0x1E,
};

enum MTK_DP_COLOR_FORMAT {
	MTK_DP_COLOR_FORMAT_RGB_444 = 0,
	MTK_DP_COLOR_FORMAT_YUV_422 = 1,
	MTK_DP_COLOR_FORMAT_YUV_444 = 2,
	MTK_DP_COLOR_FORMAT_YUV_420 = 3,
	MTK_DP_COLOR_FORMAT_YONLY = 4,
	MTK_DP_COLOR_FORMAT_RAW = 5,
	MTK_DP_COLOR_FORMAT_RESERVED = 6,
	MTK_DP_COLOR_FORMAT_DEFAULT = MTK_DP_COLOR_FORMAT_RGB_444,
	MTK_DP_COLOR_FORMAT_UNKNOWN = 15,
};

enum MTK_DP_COLOR_DEPTH {
	MTK_DP_COLOR_DEPTH_6BIT = 0,
	MTK_DP_COLOR_DEPTH_8BIT = 1,
	MTK_DP_COLOR_DEPTH_10BIT = 2,
	MTK_DP_COLOR_DEPTH_12BIT = 3,
	MTK_DP_COLOR_DEPTH_16BIT = 4,
	MTK_DP_COLOR_DEPTH_UNKNOWN = 5,
};

enum DPTX_SWING_NUM {
	DPTX_SWING0	= 0x00,
	DPTX_SWING1	= 0x01,
	DPTX_SWING2	= 0x02,
	DPTX_SWING3	= 0x03,
};

enum DPTX_PREEMPHASIS_NUM {
	DPTX_PREEMPHASIS0	= 0x00,
	DPTX_PREEMPHASIS1	= 0x01,
	DPTX_PREEMPHASIS2	= 0x02,
	DPTX_PREEMPHASIS3	= 0x03,
};

enum DPTX_Return_Status {
	DPTX_NOERR			= 0,
	DPTX_PLUG_OUT			= 1,
	DPTX_TIMEOUT			= 2,
	DPTX_AUTH_FAIL			= 3,
	DPTX_EDID_FAIL			= 4,
	DPTX_TRANING_FAIL		= 5,
	DPTX_TRANING_STATE_CHANGE	= 6,
};

enum DPTX_FEC_ERROR_COUNT_TYPE {
	FEC_ERROR_COUNT_DISABLE                = 0x0,
	FEC_UNCORRECTED_BLOCK_ERROR_COUNT      = 0x1,
	FEC_CORRECTED_BLOCK_ERROR_COUNT        = 0x2,
	FEC_BIT_ERROR_COUNT                    = 0x3,
	FEC_PARITY_BLOCK_ERROR_COUNT           = 0x4,
	FEC_PARITY_BIT_ERROR_COUNT             = 0x5,
};

enum DPTX_TRAINING_STATE {
	DPTX_NTSTATE_STARTUP		= 0x0,
	DPTX_NTSTATE_CHECKCAP		= 0x1,
	DPTX_NTSTATE_CHECKEDID		= 0x2,
	DPTX_NTSTATE_TRAINING_PRE	= 0x3,
	DPTX_NTSTATE_TRAINING		= 0x4,
	DPTX_NTSTATE_CHECKTIMING	= 0x5,
	DPTX_NTSTATE_NORMAL		= 0x6,
	DPTX_NTSTATE_POWERSAVE		= 0x7,
	DPTX_NTSTATE_DPIDLE		= 0x8,
	DPTX_NTSTATE_MAX,
};

enum DPTX_DISP_STATE {
	DPTX_DISP_NONE		= 0,
	DPTX_DISP_RESUME	= 1,
	DPTX_DISP_SUSPEND	= 2,
};

int mtk_dp_get_edid(struct mtk_dp *mtk_dp);
void mtk_dp_bridge_enable(struct mtk_dp *mtk_dp);
void mtk_dp_bridge_disable(struct mtk_dp *mtk_dp);
void mtk_dp_poweron(struct mtk_dp *mtk_dp);
int mtk_dp_hpd_event(int hpd, void *dev);

#endif /*__DP_COMMON_H__*/
