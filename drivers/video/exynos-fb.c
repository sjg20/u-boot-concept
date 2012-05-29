/*
 * LCD driver for Exynos
 *
 * Copyright (C) 2012 Samsung Electronics
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/io.h>
#include <asm/errno.h>
#include <asm/arch/clk.h>
#include <asm/arch/dsim.h>
#include <asm/arch/s5p-dp.h>
#include <asm/arch/fimd.h>
#include <asm/arch/gpio.h>
#include <asm/arch/pinmux.h>
#include <asm/arch/power.h>
#include <asm/arch/pwm.h>
#include <asm/arch/sysreg.h>
#include <lcd.h>
#include <pwm.h>
#include <asm/unaligned.h>
#include <asm/arch-exynos/spl.h>
#include "malloc.h"
#include "s5p-dp-core.h"

/* MIPI DSI Processor-to-Peripheral transaction types */
enum {
	/* Add other types here as and when required */
	MIPI_DSI_GENERIC_LONG_WRITE	= 0x29,
};

int lcd_line_length;
int lcd_color_fg;
int lcd_color_bg;

void *lcd_base;		/* Start of framebuffer memory */
void *lcd_console_address;	/* Start of console buffer */

short console_col;
short console_row;

vidinfo_t panel_info = {
	.vl_col		= LCD_XRES,
	.vl_row		= LCD_YRES,
	.vl_bpix	= LCD_COLOR16,
};

void lcd_show_board_info()
{
	return;
}

void lcd_enable()
{
	return;
}

void lcd_setcolreg(ushort regno, ushort red, ushort green, ushort blue)
{
	return;
}

/* Bypass FIMD of DISP1_BLK */
static void fimd_bypass(void)
{
	struct exynos5_sysreg *sysreg =
		(struct exynos5_sysreg *)samsung_get_base_sysreg();

	setbits_le32(&sysreg->disp1blk_cfg, FIMDBYPASS_DISP1);
}

/* Calculate the size of Framebuffer from the resolution */
ulong calc_fbsize(void)
{
	return ALIGN((panel_info.vl_col * panel_info.vl_row *
		NBITS(panel_info.vl_bpix) / 8), PAGE_SIZE);
}

/*
 * Enables the PLL of the MIPI-DSIM block.
 *
 * Depending upon the flag "enable" we will either enable the PLL or disable it
 *
 * @param dsim		pointer to the MIPI-DSIM register base address
 * @param enable	Flag which states whether or not to enable the PLL
 */
static void mipi_dsi_enable_pll(struct exynos5_dsim *dsim,
					unsigned int enable)
{
	clrbits_le32(&dsim->pllctrl, DSIM_PLL_EN_SHIFT);

	if (enable)
		setbits_le32(&dsim->pllctrl, DSIM_PLL_EN_SHIFT);
}

/*
 * Clear the MIPI-DSIM interrupt bit.
 *
 * The functions clear the interrupt bit of the DSIM interrupt status register
 *
 * @param dsim		pointer to the MIPI-DSIM register base address
 * @param int_src	Interrupt bit to clear.
 */
static void mipi_dsi_clear_interrupt(struct exynos5_dsim *dsim,
						unsigned int int_src)
{
	writel(int_src, &dsim->intsrc);
}

/*
 * Check whether D-phy generates stable byteclk.
 *
 * @param dsim	pointer to the MIPI-DSIM register base address
 * @return	non-zero when byteclk is stable; 0 otherwise
 */
static unsigned int mipi_dsi_is_pll_stable(struct exynos5_dsim *dsim)
{
	unsigned int reg;

	reg = readl(&dsim->status);

	return reg & PLL_STABLE;
}

/*
 * Write DSIM packet header.
 *
 * @param dsim		pointer to the MIPI-DSIM register base address
 * @param di		indicates the packet type
 * @param size		size of payload data in bytes
 */
static void mipi_dsi_wr_tx_header(struct exynos5_dsim *dsim,
		unsigned int di, unsigned int size)
{
	unsigned int reg = (size  << 8) | ((di & 0x3f) << 0);

	writel(reg, &dsim->pkthdr);
}

/*
 * Write whole words of DSIM packet to the payload register.
 * The caller does not need to make sure that data is aligned,
 * also the caller must ensure that reading a whole number of words
 * does not create any side-effects. This means that data always contain
 * multiple of 4 bytes, even if size is not a multiple of 4.
 *
 * @param dsim          pointer to the MIPI-DSIM register base address
 * @param data          pointer to payload data
 * @param size          size of payload data in bytes
 */
static void mipi_dsi_long_data_wr(struct exynos5_dsim *dsim,
		unsigned char *data, unsigned int size)
{
	unsigned int data_cnt, payload;

	for (data_cnt = 0; data_cnt < size; data_cnt += 4, data += 4) {
		payload = get_unaligned((u32 *)data);
		writel(payload, &dsim->payload);
	}
}

/*
 * Write MIPI-DSI payload data and packet header. Then wait till that data goes
 * out and the FIFO_EMPTY interrupt comes.
 *
 * @param dsim		pointer to the MIPI-DSIM register base address
 * @param data_id	indicates the packet type
 * @param data		pointer to payload data
 * @param size		size of payload data in bytes
 */
static void mipi_dsi_wr_data(struct exynos5_dsim *dsim,
	unsigned int data_id, unsigned char *data, unsigned int size)
{
	mipi_dsi_long_data_wr(dsim, data, size);

	/* put data into header fifo */
	mipi_dsi_wr_tx_header(dsim, data_id, size);

	/*
	 * TODO: Find out a better solution to eliminate this delay.
	 *
	 * Currently the delay is added to provide some time for the
	 * SFR payload FIFO to get empty.
	 */
	mdelay(2);
}

/*
 * Wait till all the lanes are in STOP state or we are ready to transmit HS
 * data at clock lane
 *
 * @param dsim	pointer to the MIPI-DSIM register base address
 * @return	1 if the above stated condition is true; 0 otherwise
 */
static int mipi_dsi_is_lane_state(struct exynos5_dsim *dsim)
{
	unsigned int reg = readl(&dsim->status);

	if ((reg & DSIM_STOP_STATE_DAT(0xf)) &&
		((reg & DSIM_STOP_STATE_CLK) ||
		(reg & DSIM_TX_READY_HS_CLK)))
		return 1;
	else
		return 0;
}

/*
 * Initialize the LCD panel.
 *
 * @param dsim	pointer to the MIPI-DSIM register base address
 */
static void init_lcd(struct exynos5_dsim *dsim)
{
	int i;
	unsigned char initcode[][6] = {
		{0x3c, 0x01, 0x03, 0x00, 0x02, 0x00},
		{0x14, 0x01, 0x02, 0x00, 0x00, 0x00},
		{0x64, 0x01, 0x05, 0x00, 0x00, 0x00},
		{0x68, 0x01, 0x05, 0x00, 0x00, 0x00},
		{0x6c, 0x01, 0x05, 0x00, 0x00, 0x00},
		{0x70, 0x01, 0x05, 0x00, 0x00, 0x00},
		{0x34, 0x01, 0x1f, 0x00, 0x00, 0x00},
		{0x10, 0x02, 0x1f, 0x00, 0x00, 0x00},
		{0x04, 0x01, 0x01, 0x00, 0x00, 0x00},
		{0x04, 0x02, 0x01, 0x00, 0x00, 0x00},
		{0x50, 0x04, 0x20, 0x01, 0xfa, 0x00},
		{0x54, 0x04, 0x20, 0x00, 0x50, 0x00},
		{0x58, 0x04, 0x00, 0x05, 0x30, 0x00},
		{0x5c, 0x04, 0x05, 0x00, 0x0a, 0x00},
		{0x60, 0x04, 0x20, 0x03, 0x0a, 0x00},
		{0x64, 0x04, 0x01, 0x00, 0x00, 0x00},
		{0xa0, 0x04, 0x06, 0x80, 0x44, 0x00},
		{0xa0, 0x04, 0x06, 0x80, 0x04, 0x00},
		{0x04, 0x05, 0x04, 0x00, 0x00, 0x00},
		{0x9c, 0x04, 0x0d, 0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	};

	for (i = 0; i < (ARRAY_SIZE(initcode)-1); i++)
		mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
			initcode[i], sizeof(initcode[i]));
}

/*
 * Initialize MIPI DSI
 */
static void mipi_init(void)
{
	int sw_timeout, timeout;
	unsigned int val, pms;
	struct exynos5_power *power =
		(struct exynos5_power *)samsung_get_base_power();
	struct exynos5_dsim *dsim =
		(struct exynos5_dsim *)samsung_get_base_dsim();

	/* Reset DSIM and enable MIPI_PHY1 */
	val = MIPI_PHY1_CONTROL_ENABLE | MIPI_PHY1_CONTROL_M_RESETN;
	writel(val, &power->mipi_phy1_control);

	writel(DSIM_SWRST, &dsim->swrst);

	/* Enabling data lane 0-3 */
	val = ENABLE_ALL_DATA_LANE | NUM_OF_DAT_LANE_IS_FOUR;
	writel(val, &dsim->config);

	/* Enable AFC with value 0x3 for MIPI DPHY */
	val = DSIM_PHYACCHR_AFC_CTL_VAL << DSIM_PHYACCHR_AFC_CTL_OFFSET;
	val |= DSIM_PHYACCHR_AFC_EN;
	writel(val, &dsim->phyacchr);

	pms = DSIM_PLLCTRL_PMS_VAL << DSIM_PLLCTRL_PMS_OFFSET;
	val = DSIM_FREQ_BAND << DSIM_FREQ_BAND_OFFSET;
	writel((val | pms), &dsim->pllctrl);

	writel(DSIM_PLLTMR_VAL, &dsim->plltmr);

	sw_timeout = 1000;
	mipi_dsi_clear_interrupt(dsim, PLL_STABLE);
	mipi_dsi_enable_pll(dsim, ENABLE);

	while (sw_timeout) {
		sw_timeout--;
		if (mipi_dsi_is_pll_stable(dsim))
			break;
	}

	/* Enable escape clk
	 * enable HS clk
	 * Enable Byte clk
	 * Set escape clk prescalar value to 0x90
	 */
	val = DSIM_ESC_PRESCALAR_VAL | LANE_ESC_CLK_EN_ALL |
		BYTE_CLK_EN | DSIM_ESC_CLK_EN;
	writel(val, &dsim->clkctrl);

	timeout =  100;
	/* Wait for the Data & clock lane to go in Stop state */
	while (!(mipi_dsi_is_lane_state(dsim)))
		timeout--;

	/* set_stop_state_counter */
	val = STOP_STATE_CNT_VAL << STOP_STATE_CNT_OFFSET;
	writel(val, &dsim->escmode);

	setbits_le32(&dsim->clkctrl, TXREQUEST_HS_CLK_ON);

	setbits_le32(&dsim->escmode, LP_MODE_ENABLE);

	val = (MAIN_VBP_VAL << MAIN_VBP_OFFSET) |
		(STABLE_VFP_VAL << STABLE_VFP_OFFSET) |
		(CMD_ALLOW_VAL << CMD_ALLOW_OFFSET);
	writel(val, &dsim->mvporch);

	val = (MAIN_HFP_VAL << MAIN_HFP_OFFSET) |
		(MAIN_HBP_VAL << MAIN_HBP_OFFSET);
	writel(val, &dsim->mhporch);

	val = (MAIN_HSA_VAL << MAIN_HSA_OFFSET) |
		(MAIN_VSA_VAL << MAIN_VSA_OFFSET);
	writel(val, &dsim->msync);

	val = (MAIN_VRESOL_VAL << MAIN_VRESOL_OFFSET) |
		(MAIN_HRESOL_VAL << MAIN_HRESOL_OFFSET);
	val |= MAIN_STANDBY;
	writel(val, &dsim->mdresol);

	val = readl(&dsim->config);
	val = ENABLE_ALL_DATA_LANE | NUM_OF_DAT_LANE_IS_FOUR | CLK_LANE_EN;
	val |= (RGB_565_16_BIT << MAIN_PIX_FORMAT_OFFSET);
	val |= BURST_MODE | VIDEO_MODE;
	writel(val, &dsim->config);

	writel(SFR_FIFO_EMPTY, &dsim->intsrc);
	init_lcd(dsim);
	clrbits_le32(&dsim->escmode, LP_MODE_ENABLE);
}

/*
 * Initialize display controller.
 *
 * @param lcdbase	pointer to the base address of framebuffer.
 * @pd			pointer to the main panel_data structure
 */
static void fb_init(void *lcdbase, struct exynos5_fimd_panel *pd)
{
	unsigned int val;
	ulong fbsize;
	struct exynos5_fimd *fimd =
		(struct exynos5_fimd *)samsung_get_base_fimd();
	struct exynos5_disp_ctrl *disp_ctrl =
		(struct exynos5_disp_ctrl *)samsung_get_base_disp_ctrl();

	writel(pd->ivclk | pd->fixvclk, &disp_ctrl->vidcon1);
	val = ENVID_ON | ENVID_F_ON | (pd->clkval_f << CLKVAL_F_OFFSET);
	writel(val, &fimd->vidcon0);

	val = (pd->vsync << VSYNC_PULSE_WIDTH_OFFSET) |
		(pd->lower_margin << V_FRONT_PORCH_OFFSET) |
		(pd->upper_margin << V_BACK_PORCH_OFFSET);
	writel(val, &disp_ctrl->vidtcon0);

	val = (pd->hsync << HSYNC_PULSE_WIDTH_OFFSET) |
		(pd->right_margin << H_FRONT_PORCH_OFFSET) |
		(pd->left_margin << H_BACK_PORCH_OFFSET);
	writel(val, &disp_ctrl->vidtcon1);

	val = ((pd->xres - 1) << HOZVAL_OFFSET) |
		((pd->yres - 1) << LINEVAL_OFFSET);
	writel(val, &disp_ctrl->vidtcon2);

	writel((unsigned int)lcd_base, &fimd->vidw00add0b0);

	fbsize = calc_fbsize();
	writel((unsigned int)lcd_base + fbsize, &fimd->vidw00add1b0);

	writel(pd->xres * 2, &fimd->vidw00add2);

	val = ((pd->xres - 1) << OSD_RIGHTBOTX_F_OFFSET);
	val |= ((pd->yres - 1) << OSD_RIGHTBOTY_F_OFFSET);
	writel(val, &fimd->vidosd0b);
	writel(pd->xres * pd->yres, &fimd->vidosd0c);

	setbits_le32(&fimd->shadowcon, CHANNEL0_EN);

	val = BPPMODE_F_RGB_16BIT_565 << BPPMODE_F_OFFSET;
	val |= ENWIN_F_ENABLE | HALF_WORD_SWAP_EN;
	writel(val, &fimd->wincon0);

	writel(1 << 1, &fimd->dpclkcon);
}

/*
 * Configure DP in slave mode and wait for video stream.
 * param dp		pointer to main s5p-dp structure
 * param video_info	pointer to main video_info structure.
 */
static int s5p_dp_config_video(struct s5p_dp_device *dp,
			struct video_info *video_info)
{
	int retval = 0;
	int timeout_loop = 0;
	int done_count = 0;

	s5p_dp_config_video_slave_mode(dp, video_info);

	s5p_dp_set_video_color_format(dp, video_info->color_depth,
			video_info->color_space,
			video_info->dynamic_range,
			video_info->ycbcr_coeff);

	if (s5p_dp_get_pll_lock_status(dp) == PLL_UNLOCKED) {
		debug("PLL is not locked yet.\n");
		return -EINVAL;
	}

	do {
		timeout_loop++;
		if (s5p_dp_is_slave_video_stream_clock_on(dp) == 0)
			break;
		mdelay(100);
	} while (timeout_loop < DP_TIMEOUT_LOOP_COUNT);

	if (timeout_loop >= DP_TIMEOUT_LOOP_COUNT) {
		debug("Video Stream Not on\n");
		return -1;
	}

	/* Set to use the register calculated M/N video */
	s5p_dp_set_video_cr_mn(dp, CALCULATED_M, 0, 0);

	clrbits_le32(&dp->base->video_ctl_10, FORMAT_SEL);

	/* Disable video mute */
	clrbits_le32(&dp->base->video_ctl_1, HDCP_VIDEO_MUTE);

	/* Configure video slave mode */
	s5p_dp_enable_video_master(dp);

	/* Enable video */
	setbits_le32(&dp->base->video_ctl_1, VIDEO_EN);

	timeout_loop = 0;

	do {
		timeout_loop++;
		if (s5p_dp_is_video_stream_on(dp) == 0) {
			done_count++;
			if (done_count > 10) {
				debug("s5p_dp_is_video_stream_on ok\n");
				break;
			}
		} else if (done_count) {
			done_count = 0;
		}
		mdelay(100);
	} while (timeout_loop < DP_TIMEOUT_LOOP_COUNT);

	if (timeout_loop >= DP_TIMEOUT_LOOP_COUNT) {
		debug("Video stream is not detected!\n");
		return -1;
	}

	return retval;
}

/*
 * Set DP to enhanced mode. We use this for EVT1
 * param dp	pointer to main s5p-dp structure
 */
static void s5p_dp_enable_rx_to_enhanced_mode(struct s5p_dp_device *dp)
{
	u8 data;

	s5p_dp_read_byte_from_dpcd(dp, DPCD_ADDR_LANE_COUNT_SET, &data);

	s5p_dp_write_byte_to_dpcd(dp, DPCD_ADDR_LANE_COUNT_SET,
					DPCD_ENHANCED_FRAME_EN |
					DPCD_LANE_COUNT_SET(data));
}

/*
 * Enable scrambles mode. We use this for EVT1
 * param dp	pointer to main s5p-dp structure
 */
static void s5p_dp_enable_scramble(struct s5p_dp_device *dp)
{
	u8 data;

	clrbits_le32(&dp->base->dp_training_ptn_set, SCRAMBLING_DISABLE);

	s5p_dp_read_byte_from_dpcd(dp,
		DPCD_ADDR_TRAINING_PATTERN_SET,
		&data);
	s5p_dp_write_byte_to_dpcd(dp,
		DPCD_ADDR_TRAINING_PATTERN_SET,
		(u8)(data & ~DPCD_SCRAMBLING_DISABLED));
}

/*
 * Reset DP and prepare DP for init training
 * param dp	pointer to main s5p-dp structure
 */
static int s5p_dp_init_dp(struct s5p_dp_device *dp)
{
	u32 reg;

	s5p_dp_reset(dp);

	/* SW defined function Normal operation */
	reg = readl(&dp->base->func_en_1);
	reg &= ~SW_FUNC_EN_N;
	writel(reg, &dp->base->func_en_1);

	s5p_dp_init_analog_func(dp);

	/* Init HPD */
	reg = HOTPLUG_CHG | HPD_LOST | PLUG;
	writel(reg, &dp->base->common_int_sta_4);

	reg = INT_HPD;
	writel(reg, &dp->base->int_sta_mask);

	reg = readl(&dp->base->sys_ctl_3);
	reg &= ~(F_HPD | HPD_CTRL);
	writel(reg, &dp->base->sys_ctl_3);

	s5p_dp_init_aux(dp);

	return 0;
}

/*
 * Set pre-emphasis level
 * param dp		pointer to main s5p-dp structure
 * param pre_emphasis	pre-emphasis level
 * param lane		lane number
 */
static void s5p_dp_set_lane_lane_pre_emphasis(struct s5p_dp_device *dp,
					int pre_emphasis, int lane)
{
	u32 reg;

	reg = pre_emphasis << PRE_EMPHASIS_SET_SHIFT;
	switch (lane) {
	case 0:
		writel(reg, &dp->base->ln0_link_trn_ctl);
		break;
	case 1:
		writel(reg, &dp->base->ln1_link_trn_ctl);
		break;

	case 2:
		writel(reg, &dp->base->ln2_link_trn_ctl);
		break;

	case 3:
		writel(reg, &dp->base->ln3_link_trn_ctl);
		break;
	}
}

/*
 * Read supported bandwidth type
 * param dp		pointer to main s5p-dp structure
 * param bandwidth	pointer to variable holding bandwidth type
 */
static void s5p_dp_get_max_rx_bandwidth(struct s5p_dp_device *dp,
			u8 *bandwidth)
{
	u8 data;

	/*
	 * For DP rev.1.1, Maximum link rate of Main Link lanes
	 * 0x06 = 1.62 Gbps, 0x0a = 2.7 Gbps
	 */
	s5p_dp_read_byte_from_dpcd(dp, DPCD_ADDR_MAX_LINK_RATE, &data);
	*bandwidth = data;
}

/*
 * Reset DP and prepare DP for init training
 * param dp		pointer to main s5p-dp structure
 * param lane_count	pointer to variable holding no of lanes
 */
static void s5p_dp_get_max_rx_lane_count(struct s5p_dp_device *dp,
			u8 *lane_count)
{
	u8 data;

	/*
	 * For DP rev.1.1, Maximum number of Main Link lanes
	 * 0x01 = 1 lane, 0x02 = 2 lanes, 0x04 = 4 lanes
	 */
	s5p_dp_read_byte_from_dpcd(dp, DPCD_ADDR_MAX_LANE_COUNT, &data);
	*lane_count = DPCD_MAX_LANE_COUNT(data);
}

/*
 * DP H/w Link Training. Set DPCD link rate and bandwidth.
 * param dp		pointer to main s5p-dp structure
 * param max_lane	No of lanes
 * param max_rate	bandwidth
 */
static int s5p_dp_hw_link_training(struct s5p_dp_device *dp,
				u32 max_lane,
				u32 max_rate)
{
	u32 data;
	u32 reg;
	int lane;
	int lane_count = 2;

	/* Stop Video */
	reg = readl(&dp->base->video_ctl_1);
	reg &= ~VIDEO_EN;
	writel(reg, &dp->base->video_ctl_1);

	if (s5p_dp_get_pll_lock_status(dp) == PLL_UNLOCKED) {
		debug("PLL is not locked yet.\n");
		return -1;
	}

	/* Reset Macro */
	reg = readl(&dp->base->dp_phy_test);
	reg |= MACRO_RST;
	writel(reg, &dp->base->dp_phy_test);

	/* 10 us is the minimum reset time. */
	udelay(10);

	reg &= ~MACRO_RST;
	writel(reg, &dp->base->dp_phy_test);

	/* Set TX pre-emphasis to minimum */
	for (lane = 0; lane < lane_count; lane++)
		s5p_dp_set_lane_lane_pre_emphasis(dp,
			PRE_EMPHASIS_LEVEL_0, lane);

	/* All DP analog module power up */
	writel(0x00, &dp->base->dp_phy_pd);

	/* Initialize by reading RX's DPCD */
	s5p_dp_get_max_rx_bandwidth(dp, &dp->link_train.link_rate);

	s5p_dp_get_max_rx_lane_count(dp, &dp->link_train.lane_count);

	if ((dp->link_train.link_rate != LINK_RATE_1_62GBPS) &&
		(dp->link_train.link_rate != LINK_RATE_2_70GBPS)) {
		debug("Rx Max Link Rate is abnormal :%x !\n",
			dp->link_train.link_rate);
		dp->link_train.link_rate = LINK_RATE_1_62GBPS;
	}

	if (dp->link_train.lane_count == 0) {
		debug("Rx Max Lane count is abnormal :%x !\n",
			dp->link_train.lane_count);
		dp->link_train.lane_count = (u8)LANE_COUNT1;
	}

	/* Setup TX lane count & rate */
	if (dp->link_train.lane_count > max_lane)
		dp->link_train.lane_count = max_lane;
	if (dp->link_train.link_rate > max_rate)
		dp->link_train.link_rate = max_rate;

	/* Set link rate and count as you want to establish*/
	writel(dp->video_info->lane_count, &dp->base->lane_count_set);
	writel(dp->video_info->link_rate, &dp->base->link_bw_set);

	/* Set sink to D0 (Sink Not Ready) mode. */
	s5p_dp_write_byte_to_dpcd(dp, DPCD_ADDR_SINK_POWER_STATE,
				DPCD_SET_POWER_STATE_D0);

	/* Start HW link training */
	writel(HW_TRAINING_EN, &dp->base->dp_hw_link_training);

	/* Wait unitl HW link training done */
	s5p_dp_wait_hw_link_training_done(dp);

	/* Get hardware link training status */
	data = readl(&dp->base->dp_hw_link_training);
	if (data != 0) {
		debug(" H/W link training failure: 0x%x\n", data);
		return -1;
	}

	/* Get Link Bandwidth */
	data = readl(&dp->base->link_bw_set);

	dp->link_train.link_rate = data;

	mdelay(1);
	data = readl(&dp->base->lane_count_set);
	dp->link_train.lane_count = data;

	return 0;
}

/*
 * Reset DP and prepare DP for init training
 * param dp		pointer to main s5p-dp structure
 * param count		no of lanes
 * param bwtype		badwidth type
 */
static int s5p_dp_set_link_train(struct s5p_dp_device *dp,
				u32 count,
				u32 bwtype)
{
	int i;
	int retval;

	for (i = 0; i < DP_TIMEOUT_LOOP_COUNT; i++) {
		retval = s5p_dp_hw_link_training(dp, count, bwtype);
		if (retval == 0)
			break;
	}
	return retval;
}

/*
 * Initialize DP display
 */
static int  dp_main_init()
{
	struct video_info smdk5250_dp_config = {
		.name			= "eDP-LVDS NXP PTN3460",

		.h_sync_polarity	= 0,
		.v_sync_polarity	= 0,
		.interlaced		= 0,

		.color_space		= COLOR_RGB,
		.dynamic_range		= VESA,
		.ycbcr_coeff		= COLOR_YCBCR601,
		.color_depth		= COLOR_8,

		.link_rate		= LINK_RATE_2_70GBPS,
		.lane_count		= LANE_COUNT2,
	};

	struct s5p_dp_device *dp = malloc(sizeof(struct s5p_dp_device));

	int ret, reg = 0;

	dp->base = (struct exynos5_dp *)samsung_get_base_dp();
	dp->video_info = &smdk5250_dp_config;

	clock_init_dp_clock();
	exynos_pinmux_config(PERIPH_ID_DPHPD, 0);

	power_enable_dp_phy();
	s5p_dp_init_dp(dp);

	ret = s5p_dp_set_link_train(dp, dp->video_info->lane_count,
				dp->video_info->link_rate);
	if (ret) {
		debug("unable to do link train\n");
		return -1;
	}
	s5p_dp_enable_scramble(dp);
	s5p_dp_enable_rx_to_enhanced_mode(dp);

	/* Enable enhanced mode */
	reg = readl(&dp->base->sys_ctl_4);
	reg |= ENHANCED;
	writel(reg, &dp->base->sys_ctl_4);

	writel(dp->video_info->lane_count, &dp->base->lane_count_set);
	writel(dp->video_info->link_rate, &dp->base->link_bw_set);

	s5p_dp_init_video(dp);
	ret = s5p_dp_config_video(dp, dp->video_info);
	if (ret) {
		debug("unable to config video\n");
		return -1;
	}

	return 0;
}

/*
 * Fill LCD timing data for DP or MIPI
 */
static struct exynos5_fimd_panel *fill_panel_data()
{
	struct spl_machine_param *params;
	struct exynos5_fimd_panel *panel_data;

	panel_data = malloc(sizeof(struct exynos5_fimd_panel));
	if (!panel_data) {
		debug("Failed to allocate display parameter structure\n");
		return NULL;
	}

	panel_data->xres = panel_info.vl_col;
	panel_data->yres = panel_info.vl_row;

	params->panel_type = FIMD_DP_LCD;

	if (params->panel_type == FIMD_DP_LCD) {
		panel_data->is_dp = 1;
		panel_data->is_mipi = 0;
		panel_data->fixvclk = 0;
		panel_data->ivclk = 0;
		panel_data->clkval_f = 2;
		panel_data->upper_margin = 14;
		panel_data->lower_margin = 3;
		panel_data->vsync = 5;
		panel_data->left_margin = 80;
		panel_data->right_margin = 48;
		panel_data->hsync = 32;
	} else {
		panel_data->is_dp = 0;
		panel_data->is_mipi = 1;
		panel_data->fixvclk = 1;
		panel_data->ivclk = 1;
		panel_data->clkval_f = 0xb;
		panel_data->upper_margin = 3;
		panel_data->lower_margin = 3;
		panel_data->vsync = 3;
		panel_data->left_margin = 3;
		panel_data->right_margin = 3;
		panel_data->hsync = 3;
	}
	return	panel_data;
}

void lcd_ctrl_init(void *lcdbase)
{
	struct exynos5_fimd_panel *panel_data;

	pwm_init(0, MUX_DIV_2, 0);

	panel_data = fill_panel_data();
	if (!panel_data) {
		debug("Unable to fill FIMD panel data\n");
		return;
	}

	if (panel_data->is_mipi)
		mipi_init();

	fimd_bypass();

	fb_init(lcdbase, panel_data);

	if (panel_data->is_dp) {
		if (dp_main_init())
			debug("DP initialization failed\n");
	}
}
