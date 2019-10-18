// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek dp hal driver
 *
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Tommy Chen <tommyyl.chen@mediatek.com>
 */

#include <string.h>
#include <asm/io.h>
#include <asm/system.h>
#include <linux/delay.h>

#include "mtk_dp_common.h"
#include "mtk_dp_hal.h"
#include "mtk_dp_reg.h"
#include "mtk_dp_intf.h"

/*
 * use MTK_DPTX_HAL_DEBUG to enable dsi debug logs
 * #define MTK_DPTX_HAL_DEBUG
 */

#ifdef MTK_DPTX_HAL_DEBUG
#define dptx_hal_printf(string, args...) printf("[DPTX HAL]"string, ##args)
#else
#define dptx_hal_printf(string, args...)
#endif

u32 mtk_dp_read(struct mtk_dp *mtk_dp, u32 offset)
{
	u32 read_val = 0;
	void *addr = mtk_dp->regs + offset;

	if (offset > 0x8000) {
		printf("dptx %s, error reg 0x%p, offset 0x%x\n", __func__, mtk_dp->regs, offset);
		return 0;
	}

	read_val = DPTX_REG_READ(addr) >> ((offset % 4) * 8);

	return read_val;
}

void mtk_dp_write(struct mtk_dp *mtk_dp, u32 offset, u32 val)
{
	void *addr = mtk_dp->regs + offset;

	if (offset % 4 != 0 || offset > 0x8000) {
		printf("dptx %s, error reg offset 0x%x, value 0x%x\n", __func__, offset, val);
		return;
	}

	DPTX_REG_WRITE(addr, val);
}

void mtk_dp_mask(struct mtk_dp *mtk_dp, u32 offset, u32 val, u32 mask)
{
	void *addr = mtk_dp->regs + offset;
	u32 tmp;

	if (offset % 4 != 0 || offset > 0x8000) {
		printf("dptx %s, error reg 0x%p, offset 0x%x, value 0x%x\n",
		       __func__, mtk_dp->regs, offset, val);
		return;
	}

	tmp = DPTX_REG_READ(addr);
	tmp = (tmp & ~mask) | (val & mask);
	DPTX_REG_WRITE(addr, tmp);
}

void mtk_dptx_hal_dump_reg(struct mtk_dp *mtk_dp)
{
	u32 i, val[4], reg;

	for (i = 0x0; i < 0x600; i += 16) {
		reg = 0x3000 + i;
		val[0] = mtk_dp_read(mtk_dp, reg);
		val[1] = mtk_dp_read(mtk_dp, reg + 4);
		val[2] = mtk_dp_read(mtk_dp, reg + 8);
		val[3] = mtk_dp_read(mtk_dp, reg + 12);
		printf("aux reg[0x%x] = 0x%x 0x%x 0x%x 0x%x", reg, val[0], val[1], val[2], val[3]);
	}
}

static void mtk_dp_bulk_16bit_write(struct mtk_dp *mtk_dp, u32 offset, u8 *buf,
				    u32 length)
{
	int i;
	int num_regs = (length + 1) / 2;

	for (i = 0; i < num_regs; i++) {
		u32 val = buf[i * 2] |
			  (i * 2 + 1 < length ? buf[i * 2 + 1] << 8 : 0);

		mtk_dp_write(mtk_dp, offset + i * 4, val);
	}
}

void mtk_dptx_hal_initial_setting(struct mtk_dp *mtk_dp)
{
	mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_342C,
		    XTAL_FREQ_DP_TRANS_P0_DEFAULT,
		    XTAL_FREQ_DP_TRANS_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_3540,
		    BIT(FEC_CLOCK_EN_MODE_DP_TRANS_P0_SHIFT),
		    FEC_CLOCK_EN_MODE_DP_TRANS_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_31EC,
		    BIT(AUDIO_CH_SRC_SEL_DP_ENC0_P0_SHIFT),
		    AUDIO_CH_SRC_SEL_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_304C, 0,
		    SDP_VSYNC_RISING_MASK_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_TOP_IRQ_MASK, IRQ_MASK_AUX_TOP_IRQ,
		    IRQ_MASK_AUX_TOP_IRQ);
}

void mtk_dp_msa_bypass_disable(struct mtk_dp *mtk_dp)
{
	const u16 bits_to_set =
		BIT(HTOTAL_SEL_DP_ENC0_P0_SHIFT) |
		BIT(VTOTAL_SEL_DP_ENC0_P0_SHIFT) |
		BIT(HSTART_SEL_DP_ENC0_P0_SHIFT) |
		BIT(VSTART_SEL_DP_ENC0_P0_SHIFT) |
		BIT(HWIDTH_SEL_DP_ENC0_P0_SHIFT) |
		BIT(VHEIGHT_SEL_DP_ENC0_P0_SHIFT) |
		BIT(HSP_SEL_DP_ENC0_P0_SHIFT) | BIT(HSW_SEL_DP_ENC0_P0_SHIFT) |
		BIT(VSP_SEL_DP_ENC0_P0_SHIFT) | BIT(VSW_SEL_DP_ENC0_P0_SHIFT);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3030, bits_to_set, bits_to_set);
}

void mtk_dptx_hal_set_msa(struct mtk_dp *mtk_dp)
{
	u32 va, vsync, vbp, vfp, vtotal, ha, hsync, hbp, hfp, htotal;
	struct display_mode *mode = mtk_dp->mode;

	va = mode->vdisplay;
	vsync = mode->vsync_end - mode->vsync_start;
	vbp = mode->vtotal - mode->vsync_end;
	vfp = mode->vsync_start - mode->vdisplay;

	ha = mode->hdisplay;
	hsync = mode->hsync_end - mode->hsync_start;
	hbp = mode->htotal - mode->hsync_end;
	hfp = mode->hsync_start - mode->hdisplay;

	htotal = ha + hsync + hbp + hfp;
	vtotal = va + vsync + vbp + vfp;

	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3010, htotal, HTOTAL_SW_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3018, hsync + hbp, HSTART_SW_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3028, hsync << HSW_SW_DP_ENC0_P0_SHIFT,
		    HSW_SW_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3028, 0,
		    HSP_SW_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3020, ha, HWIDTH_SW_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3014, vtotal, VTOTAL_SW_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_301C, vsync + vbp, VSTART_SW_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_302C, vsync << VSW_SW_DP_ENC0_P0_SHIFT,
		    VSW_SW_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_302C, 0,
		    VSP_SW_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3024, va, VHEIGHT_SW_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3064, ha, HDE_NUM_LAST_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3154, htotal, PGEN_HTOTAL_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3158, hfp, PGEN_HSYNC_RISING_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_315C, vsync, PGEN_HSYNC_PULSE_WIDTH_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3160, hsync + hbp, PGEN_HFDE_START_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3164, ha, PGEN_HFDE_ACTIVE_WIDTH_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3168, vtotal, PGEN_VTOTAL_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_316C, hfp, PGEN_VSYNC_RISING_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3170, vsync, PGEN_VSYNC_PULSE_WIDTH_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3174, vsync + vbp, PGEN_VFDE_START_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3178, va, PGEN_VFDE_ACTIVE_WIDTH_DP_ENC0_P0_MASK);

	dptx_hal_printf("MSA:Htt=%d Vtt=%d Hact=%d Vact=%d, fps=%d\n", htotal, vtotal,
			ha, va, mode->clock * 1000 / htotal / vtotal);
}

void mtk_dptx_hal_set_color_format(struct mtk_dp *mtk_dp, u8 color_format)
{
	u32 val;

	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3034, (color_format << MTK_DP_TEST_COLOR_FORMAT_SHIFT),
		    MTK_DP_TEST_COLOR_FORMAT_MASK);

	switch (color_format) {
	case MTK_DP_COLOR_FORMAT_YUV_422:
		val = PIXEL_ENCODE_FORMAT_DP_ENC0_P0_YCBCR422;
		break;
	case MTK_DP_COLOR_FORMAT_YUV_420:
		val = PIXEL_ENCODE_FORMAT_DP_ENC0_P0_YCBCR420;
		break;
	case MTK_DP_COLOR_FORMAT_YONLY:
	case MTK_DP_COLOR_FORMAT_RAW:
	case MTK_DP_COLOR_FORMAT_RESERVED:
	case MTK_DP_COLOR_FORMAT_UNKNOWN:
		printf("Unsupported color format: %d\n", color_format);
		fallthrough;
	case MTK_DP_COLOR_FORMAT_RGB_444:
	case MTK_DP_COLOR_FORMAT_YUV_444:
	default:
		val = PIXEL_ENCODE_FORMAT_DP_ENC0_P0_RGB;
		break;
	}

	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_303C, val,
		    PIXEL_ENCODE_FORMAT_DP_ENC0_P0_MASK);
}

void mtk_dptx_hal_set_color_depth(struct mtk_dp *mtk_dp, u8 color_depth)
{
	u32 val;

	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3034, color_depth << MTK_DP_TEST_BIT_DEPTH_SHIFT,
		    MTK_DP_TEST_BIT_DEPTH_MASK);

	switch (color_depth) {
	case MTK_DP_COLOR_DEPTH_6BIT:
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_6BIT;
		break;
	case MTK_DP_COLOR_DEPTH_8BIT:
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_8BIT;
		break;
	case MTK_DP_COLOR_DEPTH_10BIT:
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_10BIT;
		break;
	case MTK_DP_COLOR_DEPTH_12BIT:
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_12BIT;
		break;
	case MTK_DP_COLOR_DEPTH_16BIT:
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_16BIT;
		break;
	default:
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_8BIT;
		break;
	}
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_303C, val,
		    VIDEO_COLOR_DEPTH_DP_ENC0_P0_MASK);
}

void mtk_dptx_hal_set_misc(struct mtk_dp *mtk_dp, u8 misc[2])
{
	u32 val;

	val = misc[0] | misc[1] << 0x8;
	mtk_dp_write(mtk_dp, MTK_DP_ENC0_P0_3034, val);
}

u8 mtk_dptx_hal_get_color_bpp(struct mtk_dp *mtk_dp)
{
	u8 color_bpp;
	u8 dptx_color_depth = mtk_dp->info.depth;
	u8 dptx_color_format = mtk_dp->info.format;

	switch (dptx_color_depth) {
	case DP_COLOR_DEPTH_6BIT:
		if (dptx_color_format == DP_COLOR_FORMAT_YUV_422)
			color_bpp = 16;
		else if (dptx_color_format == DP_COLOR_FORMAT_YUV_420)
			color_bpp = 12;
		else
			color_bpp = 18;
		break;
	case DP_COLOR_DEPTH_8BIT:
		if (dptx_color_format == DP_COLOR_FORMAT_YUV_422)
			color_bpp = 16;
		else if (dptx_color_format == DP_COLOR_FORMAT_YUV_420)
			color_bpp = 12;
		else
			color_bpp = 24;
		break;
	case DP_COLOR_DEPTH_10BIT:
		if (dptx_color_format == DP_COLOR_FORMAT_YUV_422)
			color_bpp = 20;
		else if (dptx_color_format == DP_COLOR_FORMAT_YUV_420)
			color_bpp = 15;
		else
			color_bpp = 30;
		break;
	case DP_COLOR_DEPTH_12BIT:
		if (dptx_color_format == DP_COLOR_FORMAT_YUV_422)
			color_bpp = 24;
		else if (dptx_color_format == DP_COLOR_FORMAT_YUV_420)
			color_bpp = 18;
		else
			color_bpp = 36;
		break;
	case DP_COLOR_DEPTH_16BIT:
		if (dptx_color_format == DP_COLOR_FORMAT_YUV_422)
			color_bpp = 32;
		else if (dptx_color_format == DP_COLOR_FORMAT_YUV_420)
			color_bpp = 24;
		else
			color_bpp = 48;
		break;
	default:
		color_bpp = 24;
		printf("Set Wrong Bpp = %d\n", color_bpp);
		break;
	}

	return color_bpp;
}

void mtk_dptx_hal_set_tu_sram_rd_start(struct mtk_dp *mtk_dp, u16 value)
{
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_303C,
		    value << SRAM_START_READ_THRD_DP_ENC0_P0_SHIFT,
		    SRAM_START_READ_THRD_DP_ENC0_P0_MASK);
}

void mtk_dptx_hal_set_sdp_down_cnt_init_in_hblanking(struct mtk_dp *mtk_dp, u16 value)
{
	mtk_dp_mask(mtk_dp, MTK_DP_ENC1_P0_3364, value,
		    SDP_DOWN_CNT_INIT_IN_HBLANK_DP_ENC1_P0_MASK);
}

void mtk_dptx_hal_set_sdp_down_cnt_init(struct mtk_dp *mtk_dp, u16 value)
{
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3040, value, SDP_DOWN_CNT_INIT_DP_ENC0_P0_MASK);
}

void mtk_dptx_hal_set_tu_set_encoder(struct mtk_dp *mtk_dp)
{
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_303C,
		    BIT(VIDEO_MN_GEN_EN_DP_ENC0_P0_SHIFT),
		    VIDEO_MN_GEN_EN_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3040,
		    SDP_DOWN_CNT_INIT_VALUE << SDP_DOWN_CNT_INIT_DP_ENC0_P0_SHIFT,
		    SDP_DOWN_CNT_INIT_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC1_P0_3364,
		    SDP_DOWN_CNT_INIT_IN_HBLANK_VALUE <<
		    SDP_DOWN_CNT_INIT_IN_HBLANK_DP_ENC1_P0_SHIFT,
		    SDP_DOWN_CNT_INIT_IN_HBLANK_DP_ENC1_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC1_P0_3300,
		    VIDEO_AFIFO_RDY_SEL_VALUE << VIDEO_AFIFO_RDY_SEL_DP_ENC1_P0_SHIFT,
		    VIDEO_AFIFO_RDY_SEL_DP_ENC1_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC1_P0_3364,
		    FIFO_READ_START_POINT_VALUE << FIFO_READ_START_POINT_DP_ENC1_P0_SHIFT,
		    FIFO_READ_START_POINT_DP_ENC1_P0_MASK);
	mtk_dp_write(mtk_dp, MTK_DP_ENC1_P0_3368,
		     1 << VIDEO_SRAM_FIFO_CNT_RESET_SEL_DP_ENC1_P0_SHIFT |
		     1 << VIDEO_STABLE_CNT_THRD_DP_ENC1_P0_SHIFT |
		     BIT(SDP_DP13_EN_DP_ENC1_P0_SHIFT) |
		     1 << BS2BS_MODE_DP_ENC1_P0_SHIFT);
}

bool mtk_dptx_hal_hpd_high(struct mtk_dp *mtk_dp)
{
	bool ret = (mtk_dp_read(mtk_dp, MTK_DP_TRANS_P0_3414) & HPD_DB_DP_TRANS_P0_MASK) ?
		true : false;
	return ret;
}

static void mtk_dptx_hal_aux_irq_clear(struct mtk_dp *mtk_dp)
{
	mtk_dp_write(mtk_dp, MTK_DP_AUX_P0_3640,
		     BIT(AUX_400US_TIMEOUT_IRQ_AUX_TX_P0_SHIFT) |
		     BIT(AUX_RX_DATA_RECV_IRQ_AUX_TX_P0_SHIFT) |
		     BIT(AUX_RX_ADDR_RECV_IRQ_AUX_TX_P0_SHIFT) |
		     BIT(AUX_RX_CMD_RECV_IRQ_AUX_TX_P0_SHIFT) |
		     BIT(AUX_RX_MCCS_RECV_COMPLETE_IRQ_AUX_TX_P0_SHIFT) |
		     BIT(AUX_RX_EDID_RECV_COMPLETE_IRQ_AUX_TX_P0_SHIFT) |
		     BIT(AUX_RX_AUX_RECV_COMPLETE_IRQ_AUX_TX_P0_SHIFT));
}

static void mtk_dptx_hal_aux_set_cmd(struct mtk_dp *mtk_dp, u8 cmd, u32 addr)
{
	mtk_dp_mask(mtk_dp, MTK_DP_AUX_P0_3644, cmd,
		    MCU_REQUEST_COMMAND_AUX_TX_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_AUX_P0_3648, addr,
		    MCU_REQUEST_ADDRESS_LSB_AUX_TX_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_AUX_P0_364C, addr >> 16,
		    MCU_REQUEST_ADDRESS_MSB_AUX_TX_P0_MASK);
}

static void mtk_dptx_hal_aux_cmd_complete(struct mtk_dp *mtk_dp)
{
	mtk_dp_mask(mtk_dp, MTK_DP_AUX_P0_3650,
		    BIT(MCU_ACK_TRAN_COMPLETE_AUX_TX_P0_SHIFT),
		    MCU_ACK_TRAN_COMPLETE_AUX_TX_P0_MASK |
		    PHY_FIFO_RST_AUX_TX_P0_MASK |
		    MCU_REQ_DATA_NUM_AUX_TX_P0_MASK);
}

static void mtk_dptx_hal_aux_request_ready(struct mtk_dp *mtk_dp)
{
	mtk_dp_mask(mtk_dp, MTK_DP_AUX_P0_3630,
		    BIT(AUX_TX_REQUEST_READY_AUX_TX_P0_SHIFT),
		    AUX_TX_REQUEST_READY_AUX_TX_P0_MASK);
}

static void mtk_dptx_hal_aux_fill_write_fifo(struct mtk_dp *mtk_dp, u8 *buf, size_t length)
{
	mtk_dp_bulk_16bit_write(mtk_dp, MTK_DP_AUX_P0_3708, buf, length);
}

static void mtk_dptx_hal_aux_read_rx_fifo(struct mtk_dp *mtk_dp, u8 *buf,
					  size_t length, int read_delay)
{
	int read_pos;

	mtk_dp_mask(mtk_dp, MTK_DP_AUX_P0_3620, 0,
		    AUX_RD_MODE_AUX_TX_P0_MASK);

	for (read_pos = 0; read_pos < length; read_pos++) {
		mtk_dp_mask(mtk_dp, MTK_DP_AUX_P0_3620,
			    BIT(AUX_RX_FIFO_R_PULSE_TX_P0_SHIFT),
			    AUX_RX_FIFO_READ_PULSE_TX_P0_MASK);
		mdelay(1);
		buf[read_pos] =
			(u8)(mtk_dp_read(mtk_dp, MTK_DP_AUX_P0_3620) &
			     AUX_RX_FIFO_READ_DATA_AUX_TX_P0_MASK >>
			     AUX_RX_FIFO_READ_DATA_AUX_TX_P0_SHIFT);
	}
}

static void mtk_dptx_hal_aux_set_length(struct mtk_dp *mtk_dp, size_t length)
{
	if (length > 0) {
		mtk_dp_mask(mtk_dp, MTK_DP_AUX_P0_3650,
			    (length - 1) << MCU_REQ_DATA_NUM_AUX_TX_P0_SHIFT,
			    MCU_REQ_DATA_NUM_AUX_TX_P0_MASK);
		mtk_dp_mask(mtk_dp, MTK_DP_AUX_P0_362C, 0,
			    AUX_NO_LENGTH_AUX_TX_P0_MASK |
			    AUX_TX_AUXTX_OV_EN_AUX_TX_P0_MASK |
			    AUX_RESERVED_RW_0_AUX_TX_P0_MASK);
	} else {
		mtk_dp_mask(mtk_dp, MTK_DP_AUX_P0_362C,
			    BIT(AUX_NO_LENGTH_AUX_TX_P0_SHIFT),
			    AUX_NO_LENGTH_AUX_TX_P0_MASK |
			    AUX_TX_AUXTX_OV_EN_AUX_TX_P0_MASK |
			    AUX_RESERVED_RW_0_AUX_TX_P0_MASK);
	}
}

static int mtk_dptx_hal_aux_wait_for_completion(struct mtk_dp *mtk_dp, bool is_read)
{
	int wait_reply = MTK_DP_AUX_WAIT_REPLY_COUNT;

	while (--wait_reply) {
		u32 aux_irq_status;

		if (is_read) {
			u32 fifo_status =
				mtk_dp_read(mtk_dp, MTK_DP_AUX_P0_3618);

			if (fifo_status &
			    (AUX_RX_FIFO_WRITE_POINTER_AUX_TX_P0_MASK |
			     AUX_RX_FIFO_FULL_AUX_TX_P0_MASK)) {
				return 0;
			}
		}

		aux_irq_status = mtk_dp_read(mtk_dp, MTK_DP_AUX_P0_3640);
		if (aux_irq_status & AUX_RX_RECV_COMPLETE_IRQ_TX_P0_MASK)
			return 0;

		if (aux_irq_status & AUX_400US_TIMEOUT_IRQ_AUX_TX_P0_MASK)
			return -1;

		mdelay(1);
	}

	return -1;
}

bool mtk_dptx_hal_aux_do_transfer(struct mtk_dp *mtk_dp, bool is_read, u8 cmd,
				  u32 addr, u8 *buf, size_t length)
{
	int ret;
	u32 reply_cmd;

	if (!is_read)
		mtk_dp_mask(mtk_dp, MTK_DP_AUX_P0_3704,
			    BIT(AUX_TX_FIFO_NEW_MODE_EN_AUX_TX_P0_SHIFT),
			    AUX_TX_FIFO_NEW_MODE_EN_AUX_TX_P0_MASK);

	mtk_dptx_hal_aux_cmd_complete(mtk_dp);
	mtk_dptx_hal_aux_irq_clear(mtk_dp);
	mdelay(1);

	mtk_dptx_hal_aux_set_cmd(mtk_dp, cmd, addr);
	mtk_dptx_hal_aux_set_length(mtk_dp, length);

	if (!is_read) {
		if (length)
			mtk_dptx_hal_aux_fill_write_fifo(mtk_dp, buf, length);

		mtk_dp_mask(mtk_dp, MTK_DP_AUX_P0_3704,
			    AUX_TX_FIFO_WRITE_DATA_NEW_MODE_TOGGLE_AUX_TX_P0_MASK,
			    AUX_TX_FIFO_WRITE_DATA_NEW_MODE_TOGGLE_AUX_TX_P0_MASK);
	}

	mtk_dptx_hal_aux_request_ready(mtk_dp);

	ret = mtk_dptx_hal_aux_wait_for_completion(mtk_dp, is_read);

	reply_cmd = mtk_dp_read(mtk_dp, MTK_DP_AUX_P0_3624) &
				AUX_RX_REPLY_COMMAND_AUX_TX_P0_MASK;

	if (ret || reply_cmd) {
		u32 phy_status = mtk_dp_read(mtk_dp, MTK_DP_AUX_P0_3628) &
				 AUX_RX_PHY_STATE_AUX_TX_P0_MASK;
		if (phy_status != AUX_RX_PHY_STATE_AUX_TX_P0_RX_IDLE)
			printf("AUX Rx Aux hang, need SW reset\n");

		mtk_dptx_hal_aux_cmd_complete(mtk_dp);
		mtk_dptx_hal_aux_irq_clear(mtk_dp);

		mdelay(1);
		return false;
	}

	if (!length) {
		mtk_dp_mask(mtk_dp, MTK_DP_AUX_P0_362C, 0,
			    AUX_NO_LENGTH_AUX_TX_P0_MASK |
			    AUX_TX_AUXTX_OV_EN_AUX_TX_P0_MASK |
			    AUX_RESERVED_RW_0_AUX_TX_P0_MASK);
	} else if (is_read) {
		int read_delay;

		if (cmd == (DP_AUX_I2C_READ | DP_AUX_I2C_MOT) ||
		    cmd == DP_AUX_I2C_READ)
			read_delay = 500;
		else
			read_delay = 100;

		mtk_dptx_hal_aux_read_rx_fifo(mtk_dp, buf, length, read_delay);
	}

	mtk_dptx_hal_aux_cmd_complete(mtk_dp);
	mtk_dptx_hal_aux_irq_clear(mtk_dp);
	mdelay(1);

	return true;
}

bool mtk_dptx_hal_set_swing_preemphasis(struct mtk_dp *mtk_dp, int lane_num, int swing_value,
					int preemphasis)
{
	u32 lane_shift = lane_num * DP_TX1_VOLT_SWING_SHIFT;

	if (lane_num < 0 || lane_num > 3)
		return false;

	dptx_hal_printf("lane%d, set Swing = %x, Emp =%x\n", lane_num, swing_value, preemphasis);

	mtk_dp_mask(mtk_dp, MTK_DP_TOP_SWING_EMP,
		    swing_value << (DP_TX0_VOLT_SWING_SHIFT + lane_shift),
		    DP_TX0_VOLT_SWING_MASK << lane_shift);
	mtk_dp_mask(mtk_dp, MTK_DP_TOP_SWING_EMP,
		    preemphasis << (DP_TX0_PRE_EMPH_SHIFT + lane_shift),
		    DP_TX0_PRE_EMPH_MASK << lane_shift);

	return true;
}

bool mtk_dptx_hal_reset_swing_preemphasis(struct mtk_dp *mtk_dp)
{
	mtk_dp_mask(mtk_dp, MTK_DP_TOP_SWING_EMP, 0,
		    DP_TX0_VOLT_SWING_MASK | DP_TX1_VOLT_SWING_MASK |
		    DP_TX2_VOLT_SWING_MASK |
		    DP_TX3_VOLT_SWING_MASK |
		    DP_TX0_PRE_EMPH_MASK | DP_TX1_PRE_EMPH_MASK |
		    DP_TX2_PRE_EMPH_MASK | DP_TX3_PRE_EMPH_MASK);
	return true;
}

void mtk_dptx_hal_enable_fec(struct mtk_dp *mtk_dp, bool enable)
{
	mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_3540,
		    enable ? BIT(FEC_EN_DP_TRANS_P0_SHIFT) : 0,
		    FEC_EN_DP_TRANS_P0_MASK);
}

void mtk_dptx_hal_usbc_hpd(struct mtk_dp *mtk_dp, bool high)
{
	mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_3414,
		    HPD_OVR_EN_DP_TRANS_P0_MASK,
		    HPD_OVR_EN_DP_TRANS_P0_MASK);

	if (high)
		mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_3414,
			    HPD_SET_DP_TRANS_P0_MASK,
			    HPD_SET_DP_TRANS_P0_MASK);
	else
		mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_3414,
			    0,
			    HPD_SET_DP_TRANS_P0_MASK);

	dptx_hal_printf("MTK_DP_TRANS_P0_3414 = 0x%x\n", mtk_dp_read(mtk_dp, MTK_DP_TRANS_P0_3414));
}

u16 mtk_dptx_hal_get_sw_irq_status(struct mtk_dp *mtk_dp)
{
	return mtk_dp_read(mtk_dp, MTK_DP_TRANS_P0_35D0) & SW_IRQ_FINAL_STATUS_DP_TRANS_P0_MASK;
}

void mtk_dptx_hal_sw_interrupt_set(struct mtk_dp *mtk_dp, u16 bstatus)
{
	mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_35C0, bstatus, SW_IRQ_SET_DP_TRANS_P0_MASK);
}

void mtk_dptx_hal_sw_interrupt_clr(struct mtk_dp *mtk_dp, u16 bstatus)
{
	mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_35C8, bstatus,
		    SW_IRQ_CLR_DP_TRANS_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_35C8, 0,
		    SW_IRQ_CLR_DP_TRANS_P0_MASK);
}

void mtk_dptx_hal_sw_interrupt_enable(struct mtk_dp *mtk_dp, bool enable)
{
	if (enable)
		mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_35C4, 0,
			    SW_IRQ_MASK_DP_TRANS_P0_MASK);
	else
		mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_35C4, SW_IRQ_MASK_VALUE,
			    SW_IRQ_MASK_DP_TRANS_P0_MASK);
}

u8 mtk_dptx_hal_get_hpd_irq_status(struct mtk_dp *mtk_dp)
{
	return (mtk_dp_read(mtk_dp, MTK_DP_TRANS_P0_3418) &
			 IRQ_STATUS_DP_TRANS_P0_MASK) >>
			IRQ_STATUS_DP_TRANS_P0_SHIFT;
}

void mtk_dptx_hal_hpd_interrupt_clr(struct mtk_dp *mtk_dp, u8 bstatus)
{
	mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_3418, bstatus, IRQ_CLR_DP_TRANS_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_3418, 0, IRQ_CLR_DP_TRANS_P0_MASK);
}

void mtk_dptx_hal_hpd_interrupt_enable(struct mtk_dp *mtk_dp, bool enable)
{
	u32 val = 0;

	if (!enable)
		val = IRQ_MASK_DP_TRANS_P0_DISC_IRQ |
		      IRQ_MASK_DP_TRANS_P0_CONN_IRQ |
		      IRQ_MASK_DP_TRANS_P0_INT_IRQ;
	mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_3418, val,
		    IRQ_MASK_DP_TRANS_P0_MASK);
}

void mtk_dptx_hal_hpd_detect_setting(struct mtk_dp *mtk_dp)
{
	mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_3410, HPD_DEB_THD, HPD_DEB_THD_DP_TRANS_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_3410, HPD_INT_THD_DP_TRANS_P0_LOWER_500US |
		    HPD_INT_THD_DP_TRANS_P0_UPPER_1100US, HPD_INT_THD_DP_TRANS_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_3410,
		    (HPD_DISC_THD << HPD_DISC_THD_DP_TRANS_P0_SHIFT) |
		    (HPD_CONN_THD << HPD_CONN_THD_DP_TRANS_P0_SHIFT),
		    HPD_DISC_THD_DP_TRANS_P0_MASK | HPD_CONN_THD_DP_TRANS_P0_MASK);

	mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_3430, HPD_INT_THD_ECO_DP_TRANS_P0_HIGH_BOUND_EXT,
		    HPD_INT_THD_ECO_DP_TRANS_P0_MASK);
}

void mtk_dptx_hal_phy_setting(struct mtk_dp *mtk_dp)
{
	mtk_dp_mask(mtk_dp, MTK_DP_TOP_PWR_STATE, DP_PWR_STATE_BANDGAP_TPLL_LANE,
		    DP_PWR_STATE_MASK);

	mtk_dp_write(mtk_dp, MTK_DP_TOP_PWR_STATE, DP_PWR_STATE_BANDGAP);
	mtk_dp_write(mtk_dp, MTK_DP_PHY_DIG_BIT_RATE, BIT_RATE_RBR);
	mtk_dp_write(mtk_dp, MTK_DP_TOP_PWR_STATE, DP_PWR_STATE_BANDGAP_TPLL_LANE);

	mtk_dp_write(mtk_dp, MTK_DP_LANE0_DRIVING_PARAM_3, DRIVING_PARAM_3_DEFAULT);
	mtk_dp_write(mtk_dp, MTK_DP_LANE1_DRIVING_PARAM_3, DRIVING_PARAM_3_DEFAULT);
	mtk_dp_write(mtk_dp, MTK_DP_LANE2_DRIVING_PARAM_3, DRIVING_PARAM_3_DEFAULT);
	mtk_dp_write(mtk_dp, MTK_DP_LANE3_DRIVING_PARAM_3, DRIVING_PARAM_3_DEFAULT);

	mtk_dp_write(mtk_dp, MTK_DP_LANE0_DRIVING_PARAM_4, DRIVING_PARAM_4_DEFAULT);
	mtk_dp_write(mtk_dp, MTK_DP_LANE1_DRIVING_PARAM_4, DRIVING_PARAM_4_DEFAULT);
	mtk_dp_write(mtk_dp, MTK_DP_LANE2_DRIVING_PARAM_4, DRIVING_PARAM_4_DEFAULT);
	mtk_dp_write(mtk_dp, MTK_DP_LANE3_DRIVING_PARAM_4, DRIVING_PARAM_4_DEFAULT);

	mtk_dp_write(mtk_dp, MTK_DP_LANE0_DRIVING_PARAM_5, DRIVING_PARAM_5_DEFAULT);
	mtk_dp_write(mtk_dp, MTK_DP_LANE1_DRIVING_PARAM_5, DRIVING_PARAM_5_DEFAULT);
	mtk_dp_write(mtk_dp, MTK_DP_LANE2_DRIVING_PARAM_5, DRIVING_PARAM_5_DEFAULT);
	mtk_dp_write(mtk_dp, MTK_DP_LANE3_DRIVING_PARAM_5, DRIVING_PARAM_5_DEFAULT);

	mtk_dp_write(mtk_dp, MTK_DP_LANE0_DRIVING_PARAM_6, DRIVING_PARAM_6_DEFAULT);
	mtk_dp_write(mtk_dp, MTK_DP_LANE1_DRIVING_PARAM_6, DRIVING_PARAM_6_DEFAULT);
	mtk_dp_write(mtk_dp, MTK_DP_LANE2_DRIVING_PARAM_6, DRIVING_PARAM_6_DEFAULT);
	mtk_dp_write(mtk_dp, MTK_DP_LANE3_DRIVING_PARAM_6, DRIVING_PARAM_6_DEFAULT);

	mtk_dp_write(mtk_dp, MTK_DP_LANE0_DRIVING_PARAM_7, DRIVING_PARAM_7_DEFAULT);
	mtk_dp_write(mtk_dp, MTK_DP_LANE1_DRIVING_PARAM_7, DRIVING_PARAM_7_DEFAULT);
	mtk_dp_write(mtk_dp, MTK_DP_LANE2_DRIVING_PARAM_7, DRIVING_PARAM_7_DEFAULT);
	mtk_dp_write(mtk_dp, MTK_DP_LANE3_DRIVING_PARAM_7, DRIVING_PARAM_7_DEFAULT);

	mtk_dp_write(mtk_dp, MTK_DP_LANE0_DRIVING_PARAM_8, DRIVING_PARAM_8_DEFAULT);
	mtk_dp_write(mtk_dp, MTK_DP_LANE1_DRIVING_PARAM_8, DRIVING_PARAM_8_DEFAULT);
	mtk_dp_write(mtk_dp, MTK_DP_LANE2_DRIVING_PARAM_8, DRIVING_PARAM_8_DEFAULT);
	mtk_dp_write(mtk_dp, MTK_DP_LANE3_DRIVING_PARAM_8, DRIVING_PARAM_8_DEFAULT);

	mtk_dp_mask(mtk_dp, MTK_DP_AUX_P0_3690, BIT(RX_REPLY_COMPLETE_MODE_AUX_TX_P0_SHIFT),
		    RX_REPLY_COMPLETE_MODE_AUX_TX_P0_MASK);
}

void mtk_dptx_hal_ssc_on_off_setting(struct mtk_dp *mtk_dp, bool enable)
{
	mtk_dp_mask(mtk_dp, MTK_DP_TOP_PWR_STATE, DP_PWR_STATE_BANDGAP, DP_PWR_STATE_MASK);

	if (enable)
		mtk_dp_mask(mtk_dp, MTK_DP_PHY_DIG_PLL_CTL_1, TPLL_SSC_EN, TPLL_SSC_EN);
	else
		mtk_dp_mask(mtk_dp, MTK_DP_PHY_DIG_PLL_CTL_1, 0x0, TPLL_SSC_EN);

	mtk_dp_mask(mtk_dp, MTK_DP_TOP_PWR_STATE, DP_PWR_STATE_BANDGAP_TPLL_LANE,
		    DP_PWR_STATE_MASK);

	mdelay(1);
}

void mtk_dptx_hal_aux_setting(struct mtk_dp *mtk_dp)
{
	mtk_dp_mask(mtk_dp, MTK_DP_AUX_P0_360C, AUX_TIMEOUT_THR, AUX_TIMEOUT_THR_AUX_TX_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_AUX_P0_3658, 0, AUX_TX_OV_EN_AUX_TX_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_AUX_P0_3634,
		    AUX_TX_OVER_SAMPLE_RATE_26M << AUX_TX_OVER_SAMPLE_RATE_AUX_TX_P0_SHIFT,
		    AUX_TX_OVER_SAMPLE_RATE_AUX_TX_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_AUX_P0_3614, AUX_RX_UI_CNT_THR,
		    AUX_RX_UI_CNT_THR_AUX_TX_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_AUX_P0_37C8, BIT(MTK_ATOP_EN_AUX_TX_P0_SHIFT),
		    MTK_ATOP_EN_AUX_TX_P0_MASK);
}

void mtk_dptx_hal_digital_setting(struct mtk_dp *mtk_dp)
{
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_304C, 0,
		    VBID_VIDEO_MUTE_DP_ENC0_P0_MASK);
	mtk_dptx_hal_set_color_format(mtk_dp, DP_COLOR_FORMAT_RGB_444);

	mtk_dptx_hal_set_color_depth(mtk_dp, DP_COLOR_DEPTH_8BIT);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC1_P0_3368, 1 << BS2BS_MODE_DP_ENC1_P0_SHIFT,
		    BS2BS_MODE_DP_ENC1_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3004, BIT(DP_TX_ENCODER_4P_RESET_SW_DP_ENC0_P0_SHIFT),
		    DP_TX_ENCODER_4P_RESET_SW_DP_ENC0_P0_MASK);

	mdelay(1);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3004, 0, DP_TX_ENCODER_4P_RESET_SW_DP_ENC0_P0_MASK);
}

void mtk_dptx_hal_digital_sw_reset(struct mtk_dp *mtk_dp)
{
	mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_340C,
		    BIT(DP_TX_TRANSMITTER_4P_RESET_SW_DP_TRANS_P0_SHIFT),
		    DP_TX_TRANSMITTER_4P_RESET_SW_DP_TRANS_P0_MASK);
	mdelay(1);
	mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_340C, 0,
		    DP_TX_TRANSMITTER_4P_RESET_SW_DP_TRANS_P0_MASK);
}

void mtk_dptx_hal_phyd_reset(struct mtk_dp *mtk_dp)
{
	mtk_dp_mask(mtk_dp, MTK_DP_PHY_DIG_SW_RST, 0, DP_GLB_SW_RST_PHYD);
	mdelay(1);
	mtk_dp_mask(mtk_dp, MTK_DP_PHY_DIG_SW_RST, DP_GLB_SW_RST_PHYD, DP_GLB_SW_RST_PHYD);
}

void mtk_dptx_hal_set_tx_lane(struct mtk_dp *mtk_dp, int value)
{
	if (value == 0)
		mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_35F0, 0, TRANSMITTER_DUMMY_RW_0_MASK);
	else
		mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_35F0, TRANSMITTER_DUMMY_RW_0,
			    TRANSMITTER_DUMMY_RW_0_MASK);

	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3000, value, LANE_NUM_DP_ENC0_P0_MASK);
	mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_34A4, value << LANE_NUM_DP_TRANS_P0_SHIFT,
		    LANE_NUM_DP_TRANS_P0_MASK);
}

void mtk_dptx_hal_set_tx_rate(struct mtk_dp *mtk_dp, int value)
{
	mtk_dp_write(mtk_dp, MTK_DP_TOP_PWR_STATE, DP_PWR_STATE_BANDGAP);
	switch (value) {
	case 0x06:
		mtk_dp_write(mtk_dp, MTK_DP_PHY_DIG_BIT_RATE, BIT_RATE_RBR);
		break;
	case 0x0A:
		mtk_dp_write(mtk_dp, MTK_DP_PHY_DIG_BIT_RATE, BIT_RATE_HBR);
		break;
	case 0x14:
		mtk_dp_write(mtk_dp, MTK_DP_PHY_DIG_BIT_RATE, BIT_RATE_HBR2);
		break;
	case 0x1E:
		mtk_dp_write(mtk_dp, MTK_DP_PHY_DIG_BIT_RATE, BIT_RATE_HBR3);
		break;
	default:
		break;
	}

	mtk_dp_write(mtk_dp, MTK_DP_TOP_PWR_STATE, DP_PWR_STATE_BANDGAP_TPLL_LANE);
}

void mtk_dptx_hal_set_tx_training_pattern(struct mtk_dp *mtk_dp, int  value)
{
	if (value == TPS_1_pattern)
		mtk_dptx_hal_pht_set_idle_pattern(mtk_dp, false);

	mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_3400,
		    value ? BIT(value - 1) << PATTERN1_EN_DP_TRANS_P0_SHIFT : 0,
		    PATTERN1_EN_DP_TRANS_P0_MASK | PATTERN2_EN_DP_TRANS_P0_MASK |
		    PATTERN3_EN_DP_TRANS_P0_MASK | PATTERN4_EN_DP_TRANS_P0_MASK);
}

void mtk_dptx_hal_pht_set_idle_pattern(struct mtk_dp *mtk_dp, bool enable)
{
	const u32 val = POST_MISC_DATA_LANE0_OV_DP_TRANS_P0_MASK |
		POST_MISC_DATA_LANE1_OV_DP_TRANS_P0_MASK |
		POST_MISC_DATA_LANE2_OV_DP_TRANS_P0_MASK |
		POST_MISC_DATA_LANE3_OV_DP_TRANS_P0_MASK;
	mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_3580, enable ? val : 0, val);
}

void mtk_dptx_hal_set_ef_mode(struct mtk_dp *mtk_dp, bool  enable)
{
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3000,
		    enable ? BIT(ENHANCED_FRAME_EN_DP_ENC0_P0_SHIFT) : 0,
		    ENHANCED_FRAME_EN_DP_ENC0_P0_MASK);
}

void mtk_dptx_hal_set_scramble(struct mtk_dp *mtk_dp, bool  enable)
{
	mtk_dp_mask(mtk_dp, MTK_DP_TRANS_P0_3404,
		    enable ? DP_SCR_EN_DP_TRANS_P0_MASK : 0,
		    DP_SCR_EN_DP_TRANS_P0_MASK);
}

void mtk_dptx_hal_video_mute(struct mtk_dp *mtk_dp, bool enable)
{
	u32 val;

	val = BIT(VIDEO_MUTE_SEL_DP_ENC0_P0_SHIFT);
	if (enable)
		val |= BIT(VIDEO_MUTE_SW_DP_ENC0_P0_SHIFT);
	mtk_dp_mask(mtk_dp, MTK_DP_ENC0_P0_3000, val,
		    VIDEO_MUTE_SEL_DP_ENC0_P0_MASK |
		    VIDEO_MUTE_SW_DP_ENC0_P0_MASK);

	val = BIT(MTK_DP_TX_HDCP_CIPHER_VECTOR_BIT4);
	if (enable)
		val |= BIT(MTK_DP_TX_HDCP_CIPHER_VECTOR_BIT3);
	mtk_dp_mask(mtk_dp, MTK_DP_TX_SECURE_REG11, val,
		    MTK_DP_TX_HDCP_CIPHER_VECTOR_MASK);
}

void mtk_dptx_hal_analog_power_on_off(struct mtk_dp *mtk_dp, bool enable)
{
	if (enable) {
		mtk_dp_mask(mtk_dp, MTK_DP_TOP_RESET_AND_PROBE, 0, SW_RST_B_PHYD);
		mdelay(1);
		mtk_dp_mask(mtk_dp, MTK_DP_TOP_RESET_AND_PROBE, SW_RST_B_PHYD, SW_RST_B_PHYD);
	} else {
		mtk_dp_mask(mtk_dp, MTK_DP_TOP_PWR_STATE, 0, DP_PWR_STATE_MASK);
		mdelay(1);
		mtk_dp_write(mtk_dp, MTK_DP_0034,
			     DA_CKM_CKTX0_EN_FORCE_EN | DA_CKM_BIAS_LPF_EN_FORCE_VAL |
			     DA_CKM_BIAS_EN_FORCE_VAL |
			     DA_XTP_GLB_LDO_EN_FORCE_VAL |
			     DA_XTP_GLB_AVD10_ON_FORCE_VAL);
		mtk_dp_write(mtk_dp, MTK_DP_1040, 0);
		mtk_dp_write(mtk_dp, MTK_DP_TOP_MEM_PD,
			     MTK_DP_TOP_MEM_PD_CONTROL | BIT(FUSE_SEL_SHIFT) |
			     BIT(MEM_ISO_EN_SHIFT));
	}
}
