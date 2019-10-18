/* SPDX-License-Identifier: GPL-2.0
 *
 * MediaTek dp hal driver header
 *
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Tommy Chen <tommyyl.chen@mediatek.com>
 */

#ifndef __DPTX_HAL_H__
#define __DPTX_HAL_H__

#define AUX_CMD_I2C_R_MOT0		0x01
#define AUX_CMD_I2C_R			0x05
#define AUX_CMD_NATIVE_R		0x09
#define AUX_WAIT_REPLY_LP_CNT_NUM		20000

#define MASKBIT(a, b)			(BIT((a) + 1) - BIT((b)))

#define DP_AUX_I2C_WRITE		0x0
#define DP_AUX_I2C_READ			0x1
#define DP_AUX_I2C_WRITE_STATUS_UPDATE	0x2
#define DP_AUX_I2C_MOT			0x4
#define DP_AUX_NATIVE_WRITE		0x8
#define DP_AUX_NATIVE_READ		0x9

#define DP_AUX_NATIVE_REPLY_ACK		(0x0 << 0)
#define DP_AUX_NATIVE_REPLY_NACK	(0x1 << 0)
#define DP_AUX_NATIVE_REPLY_DEFER	(0x2 << 0)
#define DP_AUX_NATIVE_REPLY_MASK	(0x3 << 0)

#define DP_AUX_I2C_REPLY_ACK		(0x0 << 2)
#define DP_AUX_I2C_REPLY_NACK		(0x1 << 2)
#define DP_AUX_I2C_REPLY_DEFER		(0x2 << 2)
#define DP_AUX_I2C_REPLY_MASK		(0x3 << 2)

#define DPTX_REG_READ(addr) readl(addr)
#define DPTX_REG_WRITE(addr, val) writel(val, addr)

#define TPS_1_pattern 0x1
#define TPS_2_pattern 0x2
#define TPS_3_pattern 0x3
#define TPS_4_pattern 0x4

enum DPTX_LANE_NUM {
	DPTX_LANE0 = 0x0,
	DPTX_LANE1 = 0x1,
	DPTX_LANE2 = 0x2,
	DPTX_LANE3 = 0x3,
	DPTX_LANE_MAX,
};

enum DPTX_LANE_COUNT {
	DPTX_LANE_0 = 0x0,
	DPTX_LANE_2 = 0x01,
	DPTX_LANE_4 = 0x02,
};

enum DPTX_LINK_Rate {
	DPTX_RATE_RBR  = 0x06,
	DPTX_RATE_HBR  = 0x0A,
	DPTX_RATE_HBR2 = 0x14,
	DPTX_RATE_HBR3 = 0x1E,
};

enum DPTX_SDP_PKG_TYPE {
	DPTX_SDPTYP_NONE = 0x00,
	DPTX_SDPTYP_ACM  = 0x01,
	DPTX_SDPTYP_ISRC = 0x02,
	DPTX_SDPTYP_AVI  = 0x03,
	DPTX_SDPTYP_AUI  = 0x04,
	DPTX_SDPTYP_SPD  = 0x05,
	DPTX_SDPTYP_MPEG = 0x06,
	DPTX_SDPTYP_NTSC = 0x07,
	DPTX_SDPTYP_VSP  = 0x08,
	DPTX_SDPTYP_VSC  = 0x09,
	DPTX_SDPTYP_EXT  = 0x0A,
	DPTX_SDPTYP_PPS0 = 0x0B,
	DPTX_SDPTYP_PPS1 = 0x0C,
	DPTX_SDPTYP_PPS2 = 0x0D,
	DPTX_SDPTYP_PPS3 = 0x0E,
	DPTX_SDPTYP_DRM  = 0x10,
	DPTX_SDPTYP_MAX_NUM
};

enum DPTX_SDP_HB1_PKG_TYPE {
	DP_SPEC_SDPTYP_RESERVE	= 0x00,
	DP_SPEC_SDPTYP_AUDIO_TS	= 0x01,
	DP_SPEC_SDPTYP_AUDIO	= 0x02,
	DP_SPEC_SDPTYP_EXT	= 0x04,
	DP_SPEC_SDPTYP_ACM	= 0x05,
	DP_SPEC_SDPTYP_ISRC	= 0x06,
	DP_SPEC_SDPTYP_VSC	= 0x07,
	DP_SPEC_SDPTYP_CAMERA   = 0x08,
	DP_SPEC_SDPTYP_PPS      = 0x10,
	DP_SPEC_SDPTYP_EXT_VESA = 0x20,
	DP_SPEC_SDPTYP_EXT_CEA  = 0x21,
	DP_SPEC_SDPTYP_NON_AINFO = 0x80,
	DP_SPEC_SDPTYP_VS_INFO	= 0x81,
	DP_SPEC_SDPTYP_AVI_INFO	= 0x82,
	DP_SPEC_SDPTYP_SPD_INFO = 0x83,
	DP_SPEC_SDPTYP_AINFO    = 0x84,
	DP_SPEC_SDPTYP_MPG_INFO = 0x85,
	DP_SPEC_SDPTYP_NTSC_INFO = 0x86,
	DP_SPEC_SDPTYP_DRM_INFO = 0x87,
	DP_SPEC_SDPTYP_MAX_NUM
};

enum DP_COLOR_FORMAT_TYPE {
	DP_COLOR_FORMAT_RGB_444     = 0,
	DP_COLOR_FORMAT_YUV_422     = 1,
	DP_COLOR_FORMAT_YUV_444     = 2,
	DP_COLOR_FORMAT_YUV_420     = 3,
	DP_COLOR_FORMAT_YONLY       = 4,
	DP_COLOR_FORMAT_RAW         = 5,
	DP_COLOR_FORMAT_RESERVED    = 6,
	DP_COLOR_FORMAT_DEFAULT     = DP_COLOR_FORMAT_RGB_444,
	DP_COLOR_FORMAT_UNKNOWN     = 15,
};

enum DP_COLOR_DEPTH_TYPE {
	DP_COLOR_DEPTH_6BIT       = 0,
	DP_COLOR_DEPTH_8BIT       = 1,
	DP_COLOR_DEPTH_10BIT      = 2,
	DP_COLOR_DEPTH_12BIT      = 3,
	DP_COLOR_DEPTH_16BIT      = 4,
	DP_COLOR_DEPTH_UNKNOWN    = 5,
};

enum DPTX_PG_PURECOLOR {
	DPTX_PG_PURECOLOR_NONE		= 0x0,
	DPTX_PG_PURECOLOR_BLUE		= 0x1,
	DPTX_PG_PURECOLOR_GREEN		= 0x2,
	DPTX_PG_PURECOLOR_RED		= 0x3,
	DPTX_PG_PURECOLOR_MAX,
};

enum DPTX_PG_LOCATION {
	DPTX_PG_LOCATION_NONE            = 0x0,
	DPTX_PG_LOCATION_ALL             = 0x1,
	DPTX_PG_LOCATION_TOP             = 0x2,
	DPTX_PG_LOCATION_BOTTOM          = 0x3,
	DPTX_PG_LOCATION_LEFT_OF_TOP     = 0x4,
	DPTX_PG_LOCATION_LEFT_OF_BOTTOM  = 0x5,
	DPTX_PG_LOCATION_LEFT            = 0x6,
	DPTX_PG_LOCATION_RIGHT           = 0x7,
	DPTX_PG_LOCATION_LEFT_OF_LEFT    = 0x8,
	DPTX_PG_LOCATION_RIGHT_OF_LEFT   = 0x9,
	DPTX_PG_LOCATION_LEFT_OF_RIGHT   = 0xA,
	DPTX_PG_LOCATION_RIGHT_OF_RIGHT  = 0xB,
	DPTX_PG_LOCATION_MAX,
};

enum DPTX_PG_PIXEL_MASK {
	DPTX_PG_PIXEL_MASK_NONE         = 0x0,
	DPTX_PG_PIXEL_ODD_MASK          = 0x1,
	DPTX_PG_PIXEL_EVEN_MASK         = 0x2,
	DPTX_PG_PIXEL_MASK_MAX,
};

enum DPTX_PG_TYPESEL {
	DPTX_PG_NONE                    = 0x0,
	DPTX_PG_PURE_COLOR              = 0x1,
	DPTX_PG_VERTICAL_RAMPING        = 0x2,
	DPTX_PG_HORIZONTAL_RAMPING      = 0x3,
	DPTX_PG_VERTICAL_COLOR_BAR      = 0x4,
	DPTX_PG_HORIZONTAL_COLOR_BAR    = 0x5,
	DPTX_PG_CHESSBOARD_PATTERN      = 0x6,
	DPTX_PG_SUB_PIXEL_PATTERN       = 0x7,
	DPTX_PG_FRAME_PATTERN           = 0x8,
	DPTX_PG_MAX,
};

u32 mtk_dp_read(struct mtk_dp *mtk_dp, u32 offset);
void mtk_dp_mask(struct mtk_dp *mtk_dp, u32 offset, u32 val, u32 mask);
void mtk_dp_write(struct mtk_dp *mtk_dp, u32 offset, u32 val);

void mtk_dptx_hal_usbc_hpd(struct mtk_dp *mtk_dp, bool high);
void mtk_dptx_hal_dump_reg(struct mtk_dp *mtk_dp);
void mtk_dptx_hal_verify_clock(struct mtk_dp *mtk_dp);
u8 mtk_dptx_hal_get_color_bpp(struct mtk_dp *mtk_dp);
bool mtk_dptx_hal_set_swing_preemphasis(struct mtk_dp *mtk_dp, int lane_num, int swing_value,
					int preemphasis);
bool mtk_dptx_hal_reset_swing_preemphasis(struct mtk_dp *mtk_dp);
void mtk_dptx_hal_digital_sw_reset(struct mtk_dp *mtk_dp);
bool mtk_dptx_hal_hpd_high(struct mtk_dp *mtk_dp);
void mtk_dptx_hal_ssc_on_off_setting(struct mtk_dp *mtk_dp, bool enable);
void mtk_dptx_hal_sw_interrupt_set(struct mtk_dp *mtk_dp, u16 bstatus);
void mtk_dptx_hal_sw_interrupt_clr(struct mtk_dp *mtk_dp, u16 bstatus);
void mtk_dptx_hal_sw_interrupt_enable(struct mtk_dp *mtk_dp, bool enable);
void mtk_dptx_hal_hpd_interrupt_clr(struct mtk_dp *mtk_dp, u8 bstatus);
void mtk_dptx_hal_hpd_interrupt_enable(struct mtk_dp *mtk_dp, bool enable);
void mtk_dptx_hal_hpd_detect_setting(struct mtk_dp *mtk_dp);
void mtk_dptx_hal_phy_setting(struct mtk_dp *mtk_dp);
void mtk_dptx_hal_aux_setting(struct mtk_dp *mtk_dp);
void mtk_dptx_hal_adjust_phy_setting(struct mtk_dp *mtk_dp, u8 c0, u8 cp1);
void mtk_dptx_hal_digital_setting(struct mtk_dp *mtk_dp);
void mtk_dptx_hal_psctrl(bool aux_n_high_enable);
void mtk_dptx_hal_set_tx_lane(struct mtk_dp *mtk_dp, int value);
void mtk_dptx_hal_set_pgmsa(struct mtk_dp *mtk_dp, u8 address, u16 data);
void mtk_dptx_hal_pht_set_idle_pattern(struct mtk_dp *mtk_dp, bool enable);
void mtk_dptx_hal_phyd_reset(struct mtk_dp *mtk_dp);
void mtk_dptx_hal_set_tx_training_pattern(struct mtk_dp *mtk_dp, int  value);
void mtk_dptx_hal_set_ef_mode(struct mtk_dp *mtk_dp, bool  enable);
void mtk_dptx_hal_set_scramble(struct mtk_dp *mtk_dp, bool  enable);
void mtk_dptx_hal_enable_fec(struct mtk_dp *mtk_dp, bool enable);
void mtk_dptx_hal_enable_dsc(struct mtk_dp *mtk_dp, bool enable);
void mtk_dptx_hal_initial_setting(struct mtk_dp *mtk_dp);
void mtk_dptx_hal_video_mute(struct mtk_dp *mtk_dp, bool enable);
void mtk_dptx_hal_enable_bypass_msa(struct mtk_dp *mtk_dp, bool enable);

void mtk_dptx_hal_set_tu_sram_rd_start(struct mtk_dp *mtk_dp, u16 value);
void mtk_dptx_hal_set_sdp_down_cnt_init_in_hblanking(struct mtk_dp *mtk_dp, u16 value);
void mtk_dptx_hal_set_sdp_down_cnt_init(struct mtk_dp *mtk_dp, u16 value);
void mtk_dptx_hal_set_tu_set_encoder(struct mtk_dp *mtk_dp);
void mtk_dptx_hal_set_msa(struct mtk_dp *mtk_dp);
void mtk_dptx_hal_set_misc(struct mtk_dp *mtk_dp, u8 misc[2]);
void mtk_dptx_hal_set_color_depth(struct mtk_dp *mtk_dp, u8 coloer_depth);
void mtk_dptx_hal_set_color_format(struct mtk_dp *mtk_dp, u8 out_color_format);
u8 mtk_dptx_hal_get_hpd_irq_status(struct mtk_dp *mtk_dp);
u16 mtk_dptx_hal_get_sw_irq_status(struct mtk_dp *mtk_dp);
void mtk_dptx_hal_set_tx_rate(struct mtk_dp *mtk_dp, int value);
void mtk_dptx_hal_analog_power_on_off(struct mtk_dp *mtk_dp, bool enable);

#endif /* __DPTX_HAL_H__ */
