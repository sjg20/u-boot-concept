/*
 * Header file for Samsung DP (Display Port) interface driver.
 *
 * Copyright (C) 2012 Samsung Electronics Co., Ltd.
 * Author: Jingoo Han <jg1.han@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef _S5P_DP_CORE_H
#define _S5P_DP_CORE_H

#include <asm/types.h>
#include <asm/arch-exynos5/s5p-dp.h>
#include "s5p-dp.h"

struct link_train {
	int eq_loop;
	int cr_loop[4];

	u8 link_rate;
	u8 lane_count;
	u8 training_lane[4];

	enum link_training_state lt_state;
};

struct s5p_dp_device {
	unsigned int		irq;
	struct exynos5_dp	*base;
	struct video_info	*video_info;
	struct link_train	link_train;
};

/* s5p_dp_reg.c */
void s5p_dp_reset(struct s5p_dp_device *dp);
u32 s5p_dp_get_pll_lock_status(struct s5p_dp_device *dp);
void s5p_dp_init_analog_func(struct s5p_dp_device *dp);
void s5p_dp_init_aux(struct s5p_dp_device *dp);
void s5p_dp_enable_sw_function(struct s5p_dp_device *dp);
int s5p_dp_write_byte_to_dpcd(struct s5p_dp_device *dp,
				unsigned int reg_addr,
				unsigned char data);
int s5p_dp_read_byte_from_dpcd(struct s5p_dp_device *dp,
				unsigned int reg_addr,
				unsigned char *data);
int s5p_dp_write_bytes_to_dpcd(struct s5p_dp_device *dp,
				unsigned int reg_addr,
				unsigned int count,
				unsigned char data[]);
int s5p_dp_read_bytes_from_dpcd(struct s5p_dp_device *dp,
				unsigned int reg_addr,
				unsigned int count,
				unsigned char data[]);
int s5p_dp_init_video(struct s5p_dp_device *dp);

void s5p_dp_set_video_color_format(struct s5p_dp_device *dp,
				u32 color_depth,
				u32 color_space,
				u32 dynamic_range,
				u32 coeff);
int s5p_dp_is_slave_video_stream_clock_on(struct s5p_dp_device *dp);
void s5p_dp_set_video_cr_mn(struct s5p_dp_device *dp,
			enum clock_recovery_m_value_type type,
			u32 m_value,
			u32 n_value);
void s5p_dp_enable_video_master(struct s5p_dp_device *dp);
int s5p_dp_is_video_stream_on(struct s5p_dp_device *dp);
void s5p_dp_config_video_slave_mode(struct s5p_dp_device *dp,
			struct video_info *video_info);

void s5p_dp_wait_hw_link_training_done(struct s5p_dp_device *dp);
#endif /* _S5P_DP_CORE_H */
