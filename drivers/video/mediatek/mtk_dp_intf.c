// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek dp intf driver
 *
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Tommy Chen <tommyyl.chen@mediatek.com>
 */

#include <string.h>
#include <asm/io.h>
#include <asm/system.h>
#include <clk.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <errno.h>
#include <linux/err.h>
#include <video.h>
#include <video_bridge.h>

#include "mtk_dp_intf.h"

static void mtk_dpintf_mask(struct mtk_dpintf *dpintf, u32 offset, u32 val, u32 mask)
{
	void *addr = dpintf->regs + offset;
	u32 tmp = DP_INTF_REG_READ(addr) & ~mask;

	tmp |= (val & mask);
	DP_INTF_REG_WRITE(addr, tmp);
}

static void mtk_dpintf_sw_reset(struct mtk_dpintf *dpintf, bool reset)
{
	mtk_dpintf_mask(dpintf, DPINTF_RET, reset ? DP_RST : 0, DP_RST);
}

static void mtk_dpintf_enable(struct mtk_dpintf *dpintf)
{
	mtk_dpintf_mask(dpintf, DPINTF_EN, DP_EN, DP_EN);
}

static void mtk_dpintf_disable(struct mtk_dpintf *dpintf)
{
	mtk_dpintf_mask(dpintf, DPINTF_EN, 0, DP_EN);
}

static void mtk_dpintf_config_hsync(struct mtk_dpintf *dpintf, struct mtk_dpintf_sync_param *sync)
{
	mtk_dpintf_mask(dpintf, DPINTF_TGEN_HWIDTH, sync->sync_width << HPW, HPW_MASK);
	mtk_dpintf_mask(dpintf, DPINTF_TGEN_HPORCH, sync->back_porch << HBP, HBP_MASK);
	mtk_dpintf_mask(dpintf, DPINTF_TGEN_HPORCH, sync->front_porch << HFP, HFP_MASK);
}

static void mtk_dpintf_config_vsync(struct mtk_dpintf *dpintf, struct mtk_dpintf_sync_param *sync,
				    u32 width_addr, u32 porch_addr)
{
	mtk_dpintf_mask(dpintf, width_addr,
			sync->sync_width << VSYNC_WIDTH_SHIFT,
			VSYNC_WIDTH_MASK);
	mtk_dpintf_mask(dpintf, width_addr,
			sync->shift_half_line << VSYNC_HALF_LINE_SHIFT,
			VSYNC_HALF_LINE_MASK);
	mtk_dpintf_mask(dpintf, porch_addr,
			sync->back_porch << VSYNC_BACK_PORCH_SHIFT,
			VSYNC_BACK_PORCH_MASK);
	mtk_dpintf_mask(dpintf, porch_addr,
			sync->front_porch << VSYNC_FRONT_PORCH_SHIFT,
			VSYNC_FRONT_PORCH_MASK);
}

static void mtk_dpintf_config_vsync_lodd(struct mtk_dpintf *dpintf,
					 struct mtk_dpintf_sync_param *sync)
{
	mtk_dpintf_config_vsync(dpintf, sync, DPINTF_TGEN_VWIDTH, DPINTF_TGEN_VPORCH);
}

static void mtk_dpintf_config_vsync_leven(struct mtk_dpintf *dpintf,
					  struct mtk_dpintf_sync_param *sync)
{
	mtk_dpintf_config_vsync(dpintf, sync, DPINTF_TGEN_VWIDTH_LEVEN, DPINTF_TGEN_VPORCH_LEVEN);
}

static void mtk_dpintf_config_vsync_rodd(struct mtk_dpintf *dpintf,
					 struct mtk_dpintf_sync_param *sync)
{
	mtk_dpintf_config_vsync(dpintf, sync, DPINTF_TGEN_VWIDTH_RODD, DPINTF_TGEN_VPORCH_RODD);
}

static void mtk_dpintf_config_vsync_reven(struct mtk_dpintf *dpintf,
					  struct mtk_dpintf_sync_param *sync)
{
	mtk_dpintf_config_vsync(dpintf, sync, DPINTF_TGEN_VWIDTH_REVEN, DPINTF_TGEN_VPORCH_REVEN);
}

static void mtk_dpintf_config_pol(struct mtk_dpintf *dpintf,
				  struct mtk_dpintf_polarities *dpintf_pol)
{
	unsigned int pol;

	pol = (dpintf_pol->hsync_pol == MTK_DPINTF_POLARITY_RISING ? 0 : HSYNC_POL) |
	      (dpintf_pol->vsync_pol == MTK_DPINTF_POLARITY_RISING ? 0 : VSYNC_POL);
	mtk_dpintf_mask(dpintf, DPINTF_OUTPUT_SETTING, pol, HSYNC_POL | VSYNC_POL);
}

static void mtk_dpintf_config_3d(struct mtk_dpintf *dpintf, bool en_3d)
{
	mtk_dpintf_mask(dpintf, DPINTF_CON, en_3d ? TDFP_EN : 0, TDFP_EN);
}

static void mtk_dpintf_config_interface(struct mtk_dpintf *dpintf, bool inter)
{
	mtk_dpintf_mask(dpintf, DPINTF_CON, inter ? INTL_EN : 0, INTL_EN);
}

static void mtk_dpintf_config_fb_size(struct mtk_dpintf *dpintf, u32 width, u32 height)
{
	mtk_dpintf_mask(dpintf, DPINTF_SIZE, width << DP_HSIZE, HSIZE_MASK);
	mtk_dpintf_mask(dpintf, DPINTF_SIZE, height << DP_VSIZE, VSIZE_MASK);
}

static void mtk_dpintf_config_channel_limit(struct mtk_dpintf *dpintf,
					    struct mtk_dpintf_yc_limit *limit)
{
	mtk_dpintf_mask(dpintf, DPINTF_Y_LIMIT, limit->y_bottom << Y_LIMINT_BOT,
			Y_LIMINT_BOT_MASK);
	mtk_dpintf_mask(dpintf, DPINTF_Y_LIMIT, limit->y_top << Y_LIMINT_TOP,
			Y_LIMINT_TOP_MASK);
	mtk_dpintf_mask(dpintf, DPINTF_C_LIMIT, limit->c_bottom << C_LIMIT_BOT,
			C_LIMIT_BOT_MASK);
	mtk_dpintf_mask(dpintf, DPINTF_C_LIMIT, limit->c_top << C_LIMIT_TOP,
			C_LIMIT_TOP_MASK);
}

static void mtk_dpintf_config_bit_num(struct mtk_dpintf *dpintf, enum mtk_dpintf_out_bit_num num)
{
	u32 val;

	switch (num) {
	case MTK_DPINTF_OUT_BIT_NUM_8BITS:
		val = OUT_BIT_8;
		break;
	case MTK_DPINTF_OUT_BIT_NUM_10BITS:
		val = OUT_BIT_10;
		break;
	case MTK_DPINTF_OUT_BIT_NUM_12BITS:
		val = OUT_BIT_12;
		break;
	case MTK_DPINTF_OUT_BIT_NUM_16BITS:
		val = OUT_BIT_16;
		break;
	default:
		val = OUT_BIT_8;
		break;
	}
	mtk_dpintf_mask(dpintf, DPINTF_OUTPUT_SETTING, val, OUT_BIT_MASK);
}

static void mtk_dpintf_config_channel_swap(struct mtk_dpintf *dpintf,
					   enum mtk_dpintf_out_channel_swap swap)
{
	u32 val;

	switch (swap) {
	case MTK_DPINTF_OUT_CHANNEL_SWAP_RGB:
		val = SWAP_RGB;
		break;
	case MTK_DPINTF_OUT_CHANNEL_SWAP_GBR:
		val = SWAP_GBR;
		break;
	case MTK_DPINTF_OUT_CHANNEL_SWAP_BRG:
		val = SWAP_BRG;
		break;
	case MTK_DPINTF_OUT_CHANNEL_SWAP_RBG:
		val = SWAP_RBG;
		break;
	case MTK_DPINTF_OUT_CHANNEL_SWAP_GRB:
		val = SWAP_GRB;
		break;
	case MTK_DPINTF_OUT_CHANNEL_SWAP_BGR:
		val = SWAP_BGR;
		break;
	default:
		val = SWAP_RGB;
		break;
	}

	mtk_dpintf_mask(dpintf, DPINTF_OUTPUT_SETTING, val, CH_SWAP_MASK);
}

static void mtk_dpintf_config_yuv422_enable(struct mtk_dpintf *dpintf, bool enable)
{
	mtk_dpintf_mask(dpintf, DPINTF_CON, enable ? YUV422_EN : 0, YUV422_EN);
}

static void mtk_dpintf_config_color_format(struct mtk_dpintf *dpintf,
					   enum mtk_dpintf_out_color_format format)
{
	if (format == MTK_DPINTF_COLOR_FORMAT_YCBCR_444 ||
	    format == MTK_DPINTF_COLOR_FORMAT_YCBCR_444_FULL) {
		mtk_dpintf_config_yuv422_enable(dpintf, false);
		mtk_dpintf_config_channel_swap(dpintf, MTK_DPINTF_OUT_CHANNEL_SWAP_BGR);
	} else if (format == MTK_DPINTF_COLOR_FORMAT_YCBCR_422 ||
		   format == MTK_DPINTF_COLOR_FORMAT_YCBCR_422_FULL) {
		mtk_dpintf_config_yuv422_enable(dpintf, true);
		mtk_dpintf_config_channel_swap(dpintf, MTK_DPINTF_OUT_CHANNEL_SWAP_RGB);
	} else {
		mtk_dpintf_config_yuv422_enable(dpintf, false);
		mtk_dpintf_config_channel_swap(dpintf, MTK_DPINTF_OUT_CHANNEL_SWAP_RGB);
	}
}

static int mtk_dpintf_set_display_mode(struct mtk_dpintf *dpintf, struct display_mode *mode)
{
	struct mtk_dpintf_yc_limit limit;
	struct mtk_dpintf_polarities dpintf_pol;
	struct mtk_dpintf_sync_param hsync;
	struct mtk_dpintf_sync_param vsync_lodd = { 0 };
	struct mtk_dpintf_sync_param vsync_leven = { 0 };
	struct mtk_dpintf_sync_param vsync_rodd = { 0 };
	struct mtk_dpintf_sync_param vsync_reven = { 0 };

	vsync_lodd.back_porch = mode->vtotal - mode->vsync_end;
	vsync_lodd.front_porch = mode->vsync_start - mode->vdisplay;
	vsync_lodd.sync_width = mode->vsync_end - mode->vsync_start;
	vsync_lodd.shift_half_line = false;

	hsync.sync_width = (mode->hsync_end - mode->hsync_start) / 4;
	hsync.back_porch = (mode->htotal - mode->hsync_end) / 4;
	hsync.front_porch = (mode->hsync_start - mode->hdisplay) / 4;
	hsync.shift_half_line = false;

	/* let pll_rate can fix the valid range of tvdpll (1G~2GHz) */

	limit.c_bottom = 0x0000;
	limit.c_top = 0xFFF;
	limit.y_bottom = 0x0000;
	limit.y_top = 0xFFF;

	dpintf_pol.ck_pol = MTK_DPINTF_POLARITY_FALLING;
	dpintf_pol.de_pol = MTK_DPINTF_POLARITY_RISING;

	mtk_dpintf_sw_reset(dpintf, true);

	mtk_dpintf_config_hsync(dpintf, &hsync);
	mtk_dpintf_config_vsync_lodd(dpintf, &vsync_lodd);
	mtk_dpintf_config_vsync_rodd(dpintf, &vsync_rodd);
	mtk_dpintf_config_vsync_leven(dpintf, &vsync_leven);
	mtk_dpintf_config_vsync_reven(dpintf, &vsync_reven);

	mtk_dpintf_config_3d(dpintf, false);
	mtk_dpintf_config_interface(dpintf, false);
	mtk_dpintf_config_fb_size(dpintf, mode->hdisplay, mode->vdisplay);

	mtk_dpintf_config_channel_limit(dpintf, &limit);
	mtk_dpintf_config_bit_num(dpintf, dpintf->bit_num);
	mtk_dpintf_config_channel_swap(dpintf, dpintf->channel_swap);
	mtk_dpintf_config_color_format(dpintf, dpintf->color_format);

	mtk_dpintf_mask(dpintf, DPINTF_CON, INPUT_2P_EN, INPUT_2P_EN);

	mtk_dpintf_sw_reset(dpintf, false);

	return 0;
}

void dp_intf_config(struct display_mode *mode, struct mtk_dpintf_priv *dpintf)
{
	struct mtk_dpintf mtk_dpintf;

	mtk_dp_intf_init_variable(&mtk_dpintf);

	mtk_dp_intf_vclk(mode->clock * 1000);
	mtk_dpintf_set_display_mode(&mtk_dpintf, mode);
	mtk_dpintf_enable(&mtk_dpintf);
}
