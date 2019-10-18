// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek mt8188 vdosys driver
 *
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Tommy Chen <tommyyl.chen@mediatek.com>
 */

#include <asm/io.h>
#include <dm.h>
#include <dm/device_compat.h>
#include "disp_reg_mt8188.h"

#define MTK_FB_ALIGNMENT 32
#define DISP_REG_SET(addr, val) writel(val, addr)
#define DISP_REG_SET_BITS(addr, val) setbits_le32(addr, val)
#define DISP_REG_CLR_BITS(addr, val) clrbits_le32(addr, val)

#define LOGO_BYTE_PER_PIXEL (4)

/* use MTK_MAINDISP_DEBUG to enable maindisp debug logs
 * #define MTK_MAINDISP_DEBUG
 */

#ifdef MTK_MAINDISP_DEBUG
#define maindisp_printf(string, args...) printf("[MAINDISP]"string, ##args)
#else
#define maindisp_printf(string, args...)
#endif

enum {
	MTK_MAINDISP_OUTPUT_DSI,
	MTK_MAINDISP_OUTPUT_EDP,
};

static u32 _maindisp_output_width;
static u32 _maindisp_output_height;
static u32 _logobase;
static u32 _scenario = MTK_MAINDISP_OUTPUT_DSI;

struct mtk_maindisp_priv {
	void __iomem *base;
};

static void mtk_maindisp_config_ovl(u32 engine_num, u32 width, u32 height)
{
	unsigned int reg_offset;
	unsigned int val;

	reg_offset = (engine_num == 0) ? 0 : (OVL1_BASE - OVL0_BASE);

	DISP_REG_SET(reg_offset + DISP_REG_OVL_ROI_SIZE, ((width & DISP_REG_OVL_ROI_WIDTH_MASK) |
		     (height & DISP_REG_OVL_ROI_HEIGHT_MASK) << DISP_REG_OVL_ROI_HEIGHT_SHIFT));
	DISP_REG_SET(reg_offset + DISP_REG_OVL_ROI_BGCLR, DISP_REG_OVL_ROI_BGCLR_ALPHA);
	DISP_REG_SET(reg_offset + DISP_REG_OVL_SRC_CON, DISP_REG_OVL_SRC_CON_L0_EN);

	val = DISP_REG_OVL_L0_CON_CLRFMT_MAN |
		  DISP_REG_OVL_L0_CON_CLRFMT_BGRA8888 << DISP_REG_OVL_L0_CON_CLRFMT_SHIFT |
		  DISP_REG_OVL_L0_CON_CLRFMT_ALPHA_VALUE;

	DISP_REG_SET(reg_offset + DISP_REG_OVL_L0_CON, val);
	DISP_REG_SET(reg_offset + DISP_REG_OVL_L0_PITCH, width * LOGO_BYTE_PER_PIXEL);

	DISP_REG_SET(reg_offset + DISP_REG_OVL_L0_SRC_SIZE,
		     ((width & DISP_REG_OVL_L0_SRC_WIDTH_MASK) |
		     (height & DISP_REG_OVL_L0_SRC_HEIGHT_MASK) <<
		     DISP_REG_OVL_L0_SRC_HEIGHT_SHIFT));
	DISP_REG_SET(reg_offset + DISP_REG_OVL_L0_OFFSET, 0x0);
	if (engine_num == 0)
		DISP_REG_SET(reg_offset + DISP_REG_OVL_L0_ADDR, _logobase);
	else
		DISP_REG_SET(reg_offset + DISP_REG_OVL_L0_ADDR, _logobase +
					 (width * LOGO_BYTE_PER_PIXEL));

	DISP_REG_SET(reg_offset + DISP_REG_OVL_RDMA0_CTRL, DISP_REG_OVL_RDMA0_EN);

	val = DISP_REG_OVL_DATAPATH_OUTPUT_CLAMP | DISP_REG_OVL_DATAPATH_LAYER_SMI_ID_EN;
	DISP_REG_SET(reg_offset + DISP_REG_OVL_DATAPATH_CON, val);
	DISP_REG_SET(reg_offset + DISP_REG_OVL_EN, DISP_REG_OVL_EN_ENABLE);
}

static void mtk_maindisp_config_rdma(u32 engine_num, u32 width, u32 height)
{
	unsigned int reg_offset;

	reg_offset = (engine_num == 0) ? 0 : (DISP_RDMA1_BASE - DISP_RDMA0_BASE);

	DISP_REG_SET(reg_offset + DISP_REG_RDMA_GLOBAL_CON, DISP_REG_RDMA_GLOBAL_CON_ENABLE);
	DISP_REG_SET(reg_offset + DISP_REG_RDMA_SIZE_CON_0,
		     (width & DISP_REG_RDMA_SIZE_WIDTH_MASK));
	DISP_REG_SET(reg_offset + DISP_REG_RDMA_SIZE_CON_1,
		     (height & DISP_REG_RDMA_SIZE_HEIGHT_MASK));

	DISP_REG_SET(reg_offset + DISP_REG_RDMA_FIFO_CON,
		     DISP_REG_RDMA_FIFO_PSEUDO_SIZE << DISP_REG_RDMA_FIFO_PSEUDO_SIZE_SHIFT);
	DISP_REG_SET(reg_offset + DISP_REG_RDMA_MEM_GMC_SETTING_0,
		     DISP_REG_RDMA_MEM_GMC_FORCE_PREULTRA |
		     (DISP_REG_RDMA_MEM_GMC_PREULTRA_THRESH_HIGH <<
		     DISP_REG_RDMA_MEM_GMC_PREULTRA_THRESH_HIGH_SHIFT) |
		     DISP_REG_RDMA_MEM_GMC_PREULTRA_THRESH_LOW);
	DISP_REG_SET(reg_offset + DISP_REG_RDMA_MEM_GMC_SETTING_1,
		     DISP_REG_RDMA_MEM_GMC_BLOCK_ULTRA |
		     (DISP_REG_RDMA_MEM_GMC_ULTRA_THRESH_HIGH <<
		     DISP_REG_RDMA_MEM_GMC_ULTRA_THRESH_HIGH_SHIFT) |
		     DISP_REG_RDMA_MEM_GMC_ULTRA_THRESH_LOW);
	DISP_REG_SET(reg_offset + DISP_REG_RDMA_MEM_GMC_SETTING_2,
		     DISP_REG_RDMA_MEM_GMC_ISSUE_REQ_THRESHOLD);
	DISP_REG_SET(reg_offset + DISP_REG_RDMA_SRAM_SEL, DISP_REG_RDMA_SRAM_SEL_DISP_RDMA_SRAM);
	DISP_REG_SET(reg_offset + DISP_REG_RDMA_STALL_CG_CON, DISP_REG_RDMA_STALL_CG_CON_ENG_CG);
}

static void mtk_maindisp_config_color(u32 engine_num, u32 width, u32 height)
{
	unsigned int reg_offset;

	reg_offset = (engine_num == 0) ? 0 : (COLOR1_BASE - COLOR0_BASE);

	DISP_REG_SET(reg_offset + DISP_COLOR_INTERNAL_IP_WIDTH,
		     (width & DISP_COLOR_INTERNAL_IP_WIDTH_MASK));
	DISP_REG_SET(reg_offset + DISP_COLOR_INTERNAL_IP_HEIGHT,
		     (height & DISP_COLOR_INTERNAL_IP_HEIGHT_MASK));
	DISP_REG_SET(reg_offset + DISP_COLOR_CFG_MAIN,
		     DISP_COLOR_CFG_MAIN_8BIT_SWITCH | DISP_COLOR_CFG_MAIN_ALL_BYPASS);
	DISP_REG_SET(reg_offset + DISP_COLOR_CM1_EN, DISP_COLOR_CM1_EN_ENABLE);
	DISP_REG_SET(reg_offset + DISP_COLOR_CM2_EN, DISP_COLOR_CM2_EN_ENABLE);
	DISP_REG_SET(reg_offset + DISP_COLOR_START,
		     DISP_COLOR_START_DISP_8BIT_YUV | DISP_COLOR_START_DISP_COLOR_OUT_SEL |
		     DISP_COLOR_START_DISP_COLOR_START);
}

static void mtk_maindisp_config_ccorr(u32 engine_num, u32 width, u32 height)
{
	unsigned int reg_offset;

	reg_offset = (engine_num == 0) ? 0 : (CCORR1_BASE - CCORR0_BASE);

	DISP_REG_SET(reg_offset + DISP_REG_CCORR_SIZE,
		     (width & DISP_REG_CCORR_SIZE_WIDTH_MASK) << DISP_REG_CCORR_SIZE_WIDTH_SHIFT |
		     (height & DISP_REG_CCORR_SIZE_HEIGHT_MASK));
	DISP_REG_SET(reg_offset + DISP_REG_CCORR_CFG,
		     DISP_REG_CCORR_CFG_8BIT | DISP_REG_CCORR_CFG_RELAY_MODE_DISABLE);
	DISP_REG_SET(reg_offset + DISP_REG_CCORR_EN, DISP_REG_CCORR_EN_ENABLE);
}

static void mtk_maindisp_config_aal(u32 engine_num, u32 width, u32 height)
{
	unsigned int reg_offset;

	reg_offset = (engine_num == 0) ? 0 : (DISP_AAL1_BASE - DISP_AAL0_BASE);

	DISP_REG_SET(reg_offset + DISP_AAL_SIZE,
		     (width & DISP_AAL_OUTPUT_WIDTH_MASK) << DISP_AAL_OUTPUT_WIDTH_SHIFT |
		     (height & DISP_AAL_OUTPUT_HEIGHT_MASK));
	DISP_REG_SET(reg_offset + DISP_AAL_OUTPUT_SIZE,
		     (width & DISP_AAL_OUTPUT_WIDTH_MASK) << DISP_AAL_OUTPUT_WIDTH_SHIFT |
		     (height & DISP_AAL_OUTPUT_HEIGHT_MASK));
	DISP_REG_SET(reg_offset + DISP_AAL_EN, DISP_AAL_EN_ENABLE);
	DISP_REG_SET(reg_offset + DISP_AAL_CFG,
		     DISP_AAL_CF_8BIT_SWITCH | DISP_AAL_CF_CG_DISABLE | DISP_AAL_CF_RELAY_MODE);
}

static void mtk_maindisp_config_gamma(u32 engine_num, u32 width, u32 height)
{
	unsigned int reg_offset;

	reg_offset = (engine_num == 0) ? 0 : (DISP_GAMMA1_BASE - DISP_GAMMA0_BASE);

	DISP_REG_SET(reg_offset + DISP_REG_GAMMA_SIZE,
		     (width & DISP_REG_GAMMA_WIDTH_MASK) << DISP_REG_GAMMA_WIDTH_SHIFT |
		     (height & DISP_REG_GAMMA_HEIGHT_MASK));
	DISP_REG_SET(reg_offset + DISP_REG_GAMMA_CFG,
		     DISP_REG_GAMMA_CFG_STALL_CG_ON | DISP_REG_GAMMA_RELAY_MODE_ENABLE);
	DISP_REG_SET(reg_offset + DISP_REG_GAMMA_EN, DISP_REG_GAMMA_EN_ENABLE);
}

static void mtk_maindisp_config_postmask(u32 engine_num, u32 width, u32 height)
{
	unsigned int reg_offset = 0;

	DISP_REG_SET(reg_offset + DISP_REG_POSTMASK_SIZE,
		     (width & DISP_REG_POSTMASK_SIZE_WIDTH_MASK) <<
		     DISP_REG_POSTMASK_SIZE_WIDTH_SHIFT |
		     (height & DISP_REG_POSTMASK_SIZE_HEIGHT_MASK));
	DISP_REG_SET(reg_offset + DISP_REG_POSTMASK_CFG,
		     DISP_REG_POSTMASK_CFG_STALL_CG_ON | DISP_REG_POSTMASK_CFG_GCLAST_ENABLE |
		     DISP_REG_POSTMASK_CFG_BGCLR_IN_SEL | DISP_REG_POSTMASK_CFG_DRAM_MODE |
		     DISP_REG_POSTMASK_CFG_RELAY_MODE_ENABLE);
	DISP_REG_SET(reg_offset + DISP_REG_POSTMASK_EN, DISP_REG_POSTMASK_EN_ENABLE);
}

static void mtk_maindisp_config_dither(u32 engine_num, u32 width, u32 height)
{
	unsigned int reg_offset;

	reg_offset = (engine_num == 0) ? 0 : (DITHER1_BASE - DITHER0_BASE);

	DISP_REG_SET(reg_offset + DISP_REG_DITHER_SIZE,
		     (width & DISP_REG_DITHER_SIZE_WIDTH_MASK) <<
		     DISP_REG_DITHER_SIZE_WIDTH_SHIFT |
		     (height & DISP_REG_DITHER_SIZE_HEIGHT_MASK));
	DISP_REG_SET(reg_offset + DISP_REG_DITHER_CFG, DISP_REG_DITHER_CFG_RELAY_MODE_ENABLE);
	DISP_REG_SET(reg_offset + DISP_REG_DITHER_EN, DISP_REG_DITHER_EN_ENABLE);
	DISP_REG_SET(reg_offset + DISP_REG_DITHER_14, DISP_REG_DITHER_14_TESTPIN_INPUT_ENABLE);
	DISP_REG_SET(reg_offset + DISP_REG_DITHER_6,
		     DISP_REG_DITHER_6_FPHASE_R | DISP_REG_DITHER_6_FPHASE_EN |
		     DISP_REG_DITHER_6_RDITHER_EN);
	DISP_REG_SET(reg_offset + DISP_REG_DITHER_5, DISP_REG_DITHER_5_SHORT_LINE_LENGTH_VALUE);
}

static void mtk_maindisp_config_merge(u32 width, u32 height)
{
	DISP_REG_SET(DISP_MERGE_CFG_0, ((height << DISP_MERGE_CFG_HEIGHT_SHIFT) | width));
	DISP_REG_SET(DISP_MERGE_CFG_1, ((height << DISP_MERGE_CFG_HEIGHT_SHIFT) | width));

	DISP_REG_SET(DISP_MERGE_CFG_4, ((height << DISP_MERGE_CFG_HEIGHT_SHIFT) | width));
	DISP_REG_SET(DISP_MERGE_CFG_12, DISP_MERGE_CFG_12_CFG_10_10_1PI_2PO_BUF_MODE);

	DISP_REG_SET(DISP_MERGE_CFG_24, ((height << DISP_MERGE_CFG_HEIGHT_SHIFT) | width));
	DISP_REG_SET(DISP_MERGE_CFG_25, ((height << DISP_MERGE_CFG_HEIGHT_SHIFT) | width));
	DISP_REG_SET(DISP_MERGE_CFG_26, ((height << DISP_MERGE_CFG_HEIGHT_SHIFT) | width));
	DISP_REG_SET(DISP_MERGE_CFG_27, ((height << DISP_MERGE_CFG_HEIGHT_SHIFT) | width));
	DISP_REG_SET(DISP_MERGE_ENABLE, DISP_MERGE_ENABLE_ENABLE);
}

static void mtk_maindisp_config_mux(void)
{
	u32 width = 0, height = 0;

	mtk_maindisp_get_w_h(&width, &height);

	DISP_REG_SET(MT8188_VDO0_OVL_MOUT_EN, MT8188_VDO0_OVL_MOUT_EN_OUTPUT_TO_RDMA0);
	DISP_REG_SET(MT8188_VDO0_DISP_RDMA_SEL, MT8188_VDO0_DISP_RDMA_SEL_TO_COLOR0);

	switch (_scenario) {
	case MTK_MAINDISP_OUTPUT_DSI:
		DISP_REG_SET(MT8188_VDO0_DSI0_SEL, MT8188_VDO0_DSI0_IN_FROM_DITHER0_OUT);
		DISP_REG_SET(MT8188_VDO0_DISP_DITHER0_SEL, MT8188_VDO0_DISP_DITHER0_OUT_TO_DSI0);
		DISP_REG_SET(MT8188_VDO0_VPP_MERGE_SEL, MT8188_VDO0_VPP_MERGE_IN_FROM_DSC_WRAP0);
		DISP_REG_SET(MT8188_VDO0_DSC_WRAP_SEL, MT8188_VDO0_DSC_WRAP_IN_FROM_DITHER0);
		break;
	case MTK_MAINDISP_OUTPUT_EDP:
		DISP_REG_SET(MT8188_VDO0_DP_INTF0_SEL, MT8188_VDO0_DP_INTF0_IN_FROM_VPP_MERGE0);
		DISP_REG_SET(MT8188_VDO0_VPP_MERGE_SEL,
			     (MT8188_VDO0_VPP_MERGE_OUT_TO_DP_INTF0 <<
			     MT8188_VDO0_VPP_MERGE_OUT_SHIFT) |
			     MT8188_VDO0_VPP_MERGE_IN_FROM_DSC_WRAP0);
		DISP_REG_SET(MT8188_VDO0_DSC_WRAP_SEL,
			     (MT8188_VDO0_DSC_WRAP_OUT_TO_VPP_MERGE0 <<
			     MT8188_VDO0_DSC_WRAP_OUT_SHIFT) |
			     MT8188_VDO0_DSC_WRAP_IN_FROM_DITHER0);
		break;
	}
}

static void mtk_maindisp_config_mutex(void)
{
	u32 val;

	val = DISP_MUTEX0_VDO0_DISP_POSTMASK0 |
		  DISP_MUTEX0_VDO0_VPP_MERGE0 |
		  DISP_MUTEX0_VDO0_DSC_WRAP0 |
		  DISP_MUTEX0_VDO0_DISP_DITHER0 |
		  DISP_MUTEX0_VDO0_DISP_GAMMA0 |
		  DISP_MUTEX0_VDO0_DISP_AAL0 |
		  DISP_MUTEX0_VDO0_DISP_CCORR0 |
		  DISP_MUTEX0_VDO0_DISP_COLOR0 |
		  DISP_MUTEX0_VDO0_DISP_RDMA0 |
		  DISP_MUTEX0_VDO0_DISP_OVL0;

	switch (_scenario) {
	case MTK_MAINDISP_OUTPUT_DSI:
		val |= DISP_MUTEX0_VDO0_DSI0;
		DISP_REG_SET(DISP_MUTEX0_MOD0, val);
		DISP_REG_SET(DISP_MUTEX0_CTL,
			     DISP_MUTEX0_MUTEX_EOF_DSI0 << DISP_MUTEX0_MUTEX_EOF_SHIFT |
			     DISP_MUTEX0_MUTEX_SOF_DSI0);
		break;
	case MTK_MAINDISP_OUTPUT_EDP:
		val |= DISP_MUTEX0_VDO0_DP_INTF0 | DISP_MUTEX0_VDO0_DSC_WRAP0;
		DISP_REG_SET(DISP_MUTEX0_MOD0, val);
		DISP_REG_SET(DISP_MUTEX0_CTL,
			     DISP_MUTEX0_MUTEX_EOF_DP_INTF_EDP << DISP_MUTEX0_MUTEX_EOF_SHIFT |
			     DISP_MUTEX0_MUTEX_SOF_DP_INTF_EDP);
		break;
	}
	DISP_REG_SET(DISP_MUTEX0_EN, DISP_MUTEX0_EN_ENABLE);
}

static void mtk_maindisp_init(void)
{
	u32 val;

	val = MT8188_VDO0_OVL_GLOBAL_CG_B29_DISP_COLOR0	|
		  MT8188_VDO0_OVL_GLOBAL_CG_B28_APB_BUS |
		  MT8188_VDO0_OVL_GLOBAL_CG_B26_DISP_AAL0 |
		  MT8188_VDO0_OVL_GLOBAL_CG_B19_DISP_RDMA0 |
		  MT8188_VDO0_OVL_GLOBAL_CG_B10_DISP_DITHER0 |
		  MT8188_VDO0_OVL_GLOBAL_CG_B8_DISP_GAMMA0 |
		  MT8188_VDO0_OVL_GLOBAL_CG_B6_DISP_MUTEX0 |
		  MT8188_VDO0_OVL_GLOBAL_CG_B4_DISP_CCORR0 |
		  MT8188_VDO0_OVL_GLOBAL_CG_B0_DISP_OVL0;

	/* config VDOSYS0 CG according to scenario*/
	switch (_scenario) {
	case MTK_MAINDISP_OUTPUT_DSI:
		val |= MT8188_VDO0_OVL_GLOBAL_CG_B21_DSI0;
		break;
	case MTK_MAINDISP_OUTPUT_EDP:
		val |= MT8188_VDO0_OVL_GLOBAL_CG_B25_DP_INTF0;
		break;
	}
	DISP_REG_SET(MT8188_VDO0_OVL_GLOBAL_CG_CLR0, val);

	val = MT8188_VDO0_OVL_GLOBAL_CG_B15_SMI_RSI |
		  MT8188_VDO0_OVL_GLOBAL_CG_B14_SMI_LARM |
		  MT8188_VDO0_OVL_GLOBAL_CG_B13_SMI_IOMMU |
		  MT8188_VDO0_OVL_GLOBAL_CG_B12_SMI_EMI |
		  MT8188_VDO0_OVL_GLOBAL_CG_B11_SMI_COMMON |
		  MT8188_VDO0_OVL_GLOBAL_CG_B10_SMI_GALS |
		  MT8188_VDO0_OVL_GLOBAL_CG_B0_DISP_POSTMASK0;

	DISP_REG_SET(MT8188_VDO0_OVL_GLOBAL_CG_CLR1, val);
	DISP_REG_SET(DISPSYS_SMI_LARB0_FORCE_ULTRA, DISPSYS_SMI_LARB0_FORCE_ALL_ULTRA);
}

void mtk_maindisp_reset(void)
{
	/* reset according to scenario */
	switch (_scenario) {
	case MTK_MAINDISP_OUTPUT_DSI:
		DISP_REG_CLR_BITS(MT8188_VDO0_OVL_GLOBAL0_SW0_RST_B,
				  MT8188_VDO0_OVL_GLOBAL_CG_B21_DSI0);
		DISP_REG_SET_BITS(MT8188_VDO0_OVL_GLOBAL0_SW0_RST_B,
				  MT8188_VDO0_OVL_GLOBAL_CG_B21_DSI0);
		break;
	case MTK_MAINDISP_OUTPUT_EDP:
		DISP_REG_CLR_BITS(MT8188_VDO0_OVL_GLOBAL0_SW0_RST_B,
				  MT8188_VDO0_OVL_GLOBAL_CG_B25_DP_INTF0);
		DISP_REG_SET_BITS(MT8188_VDO0_OVL_GLOBAL0_SW0_RST_B,
				  MT8188_VDO0_OVL_GLOBAL_CG_B25_DP_INTF0);
		break;
	}
}

void mtk_maindisp_set_w_h(u32 width, u32 height)
{
	_maindisp_output_width = width;
	_maindisp_output_height = height;
}

void mtk_maindisp_get_w_h(u32 *width, u32 *height)
{
	*width = _maindisp_output_width;
	*height = _maindisp_output_height;
}

void mt_maindisp_set_fb_addr(u32 addr)
{
	_logobase = addr;
}

u32 mt_maindisp_get_fb_addr(void)
{
	return _logobase;
}

void mt_maindisp_set_scenario_edp(void)
{
	_scenario = MTK_MAINDISP_OUTPUT_EDP;
}

void mtk_maindisp_update(u32 x, u32 y, u32 width, u32 height)
{
	maindisp_printf("%s x = %d y = %d width = %d height = %d\n", __func__, x, y, width, height);

	if (width == 0 || height == 0)
		return;
	mtk_maindisp_init();
	mtk_maindisp_config_mux();

	mtk_maindisp_config_ovl(0, width, height);
	mtk_maindisp_config_rdma(0, width, height);
	mtk_maindisp_config_color(0, width, height);
	mtk_maindisp_config_ccorr(0, width, height);
	mtk_maindisp_config_aal(0, width, height);
	mtk_maindisp_config_gamma(0, width, height);
	mtk_maindisp_config_postmask(0, width, height);
	mtk_maindisp_config_dither(0, width, height);
	mtk_maindisp_config_merge(width, height);

	mtk_maindisp_config_mutex();
}

static int mtk_maindisp_probe(struct udevice *dev)
{
	struct mtk_maindisp_priv *maindisp = dev_get_priv(dev);

	maindisp->base = dev_remap_addr(dev);
	if (IS_ERR(maindisp->base))
		return PTR_ERR(maindisp->base);

	return 0;
}

static const struct udevice_id mtk_maindisp_ids[] = {
	{ .compatible = "mediatek,mt8188-mmsys" },
	{}
};

U_BOOT_DRIVER(mtk_maindisp) = {
	.name	   = "mtk_maindisp",
	.id	   = UCLASS_MISC,
	.of_match  = mtk_maindisp_ids,
	.probe	   = mtk_maindisp_probe,
	.priv_auto = sizeof(struct mtk_maindisp_priv),
};
