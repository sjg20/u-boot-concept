// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek mt8188 dp setting
 *
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Tommy Chen <tommyyl.chen@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include "mtk_dp_common.h"
#include "mtk_dp_hal.h"
#include "mtk_dp_intf.h"
#include "disp_reg_mt8188.h"

#define DP_CLOCK_REG_READ(addr) readl(addr)
#define DP_CLOCK_REG_WRITE(addr, val) writel(val, addr)

/* DP platform functions */
void mtk_dptx_init_variable(struct mtk_dp *mtk_dp)
{
	mtk_dp->regs = (void *)EDPTX_BASE;

	mtk_dp->training_info.sys_max_link_rate = DP_LINKRATE_HBR3;
	mtk_dp->training_info.link_rate = DP_LINKRATE_HBR2;
	mtk_dp->training_info.link_lane_count = DP_LANECOUNT_4;
	mtk_dp->training_info.sink_extcap_en = false;
	mtk_dp->training_info.sink_ssc_en = false;
	mtk_dp->training_info.tps3 = true;
	mtk_dp->training_info.tps4 = true;
	mtk_dp->training_state = DPTX_NTSTATE_STARTUP;

	mtk_dp->info.format = DP_COLOR_FORMAT_RGB_444;
	mtk_dp->info.depth = DP_COLOR_DEPTH_8BIT;

	mtk_dp->power_on = false;
	mtk_dp->video_enable = false;
	mtk_dp->dp_ready = false;
	mtk_dp->has_dsc = false;
	mtk_dp->has_fec = false;
	mtk_dp->dsc_enable = false;
}

void mtk_dp_intf_init_variable(struct mtk_dpintf *mtk_dpintf)
{
	mtk_dpintf->regs = (void *)(DP_INTF0_BASE);

	mtk_dpintf->color_format = MTK_DPINTF_COLOR_FORMAT_RGB;
	mtk_dpintf->yc_map = MTK_DPINTF_OUT_YC_MAP_RGB;
	mtk_dpintf->bit_num = MTK_DPINTF_OUT_BIT_NUM_8BITS;
	mtk_dpintf->channel_swap = MTK_DPINTF_OUT_CHANNEL_SWAP_RGB;
};

void mtk_dp_intf_vclk(u32 pixel_clk_freq)
{
	u32 clksrc = 0;
	u32 val = 0;
	u32 div;
	u32 pcw, pll_clk;

	if (pixel_clk_freq < CLOCK_74MHZ) {
		pll_clk = pixel_clk_freq * 4;
		clksrc = TVDPLL_D16;
	} else if (pixel_clk_freq < CLOCK_148MHZ) {
		pll_clk = pixel_clk_freq * 2;
		clksrc = TVDPLL_D8;
	} else if (pixel_clk_freq < CLOCK_594MHZ) {
		pll_clk = pixel_clk_freq;
		clksrc = TVDPLL_D4;
	} else {
		pll_clk = CLOCK_594MHZ;
		clksrc = TVDPLL_D4;
	}

	if ((CLOCK_2376MHZ / pll_clk) > 8)
		div = RG_TVDPLL1_POSDIV_DIV_16;
	else if ((CLOCK_2376MHZ / pll_clk) > 4)
		div = RG_TVDPLL1_POSDIV_DIV_8;
	else if ((CLOCK_2376MHZ / pll_clk) > 2)
		div = RG_TVDPLL1_POSDIV_DIV_4;
	else if ((CLOCK_2376MHZ / pll_clk) > 1)
		div = RG_TVDPLL1_POSDIV_DIV_2;
	else
		div = RG_TVDPLL1_POSDIV_DIV_1;

	pcw = ((pll_clk / CLOCK_1MHZ) * (1 << div)) << 14;
	pcw /= CLOCK_26MHZ_FACTOR;

	val = RG_TVDPLL1_CK_EN | RG_TVDPLL1_BW | RG_TVDPLL1_SDM_FRA_EN |
		RG_TVDPLL1_GLITCH_FREE_EN | RG_TVDPLL1_CK_VPROC_EN;

	DP_CLOCK_REG_WRITE(TVDPLL_CON0, val);
	DP_CLOCK_REG_WRITE(TVDPLL_CON3, DP_CLOCK_REG_READ(TVDPLL_CON3) | RG_TVDPLL1_SDM_PWR_ON);
	udelay(100);
	DP_CLOCK_REG_WRITE(TVDPLL_CON3, DP_CLOCK_REG_READ(TVDPLL_CON3) &
			   (~(RG_TVDPLL1_SDM_ISO_EN)));
	udelay(100);

	DP_CLOCK_REG_WRITE(TVDPLL_CON1, div << RG_TVDPLL1_POSDIV_SHIFT |
		(pcw & RG_TVDPLL1_SDM_PCW_MASK));

	val |= RG_TVDPLL1_EN;
	DP_CLOCK_REG_WRITE(TVDPLL_CON0, val);

	DP_CLOCK_REG_WRITE(CLK_CFG_9_CLR, CLK_HF_FEDP_CK_SEL_MASK);
	DP_CLOCK_REG_WRITE(CLK_CFG_9_SET,
			   (clksrc << CLK_HF_FEDP_CK_SEL_SHIFT) & CLK_HF_FEDP_CK_SEL_MASK);
	DP_CLOCK_REG_WRITE(CLK_CFG_9_CLR, PDN_HF_FEDP_CK);
	DP_CLOCK_REG_WRITE(CLK_CFG_UPDATE_1, HF_FEDP_CK_UPDATE);
}
