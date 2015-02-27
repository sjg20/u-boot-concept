/*
 * (C) Copyright 2015 Google, Inc
 * Copyright 2014 Rockchip Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 * From coreboot file of the same name
 */

#ifndef _ASM_ARCH_GRF_H
#define _ASM_ARCH_GRF_H

struct rk3288_grf_gpio_lh {
	u32 l;
	u32 h;
};

struct rk3288_grf_regs {
	u32 reserved[3];
	union {
		u32 gpio1d_iomux;
		u32 iomux_lcdc;
	};
	u32 gpio2a_iomux;
	u32 gpio2b_iomux;
	union {
		u32 gpio2c_iomux;
		u32 iomux_i2c3;
	};
	u32 reserved2;
	union {
		u32 gpio3a_iomux;
		u32 iomux_emmcdata;
	};
	union {
		u32 gpio3b_iomux;
		u32 iomux_emmcpwren;
	};
	union {
		u32 gpio3c_iomux;
		u32 iomux_emmccmd;
	};
	u32 gpio3dl_iomux;
	u32 gpio3dh_iomux;
	u32 gpio4al_iomux;
	u32 gpio4ah_iomux;
	u32 gpio4bl_iomux;
	u32 reserved3;
	u32 gpio4c_iomux;
	u32 gpio4d_iomux;
	u32 reserved4;
	union {
		u32 gpio5b_iomux;
		u32 iomux_spi0;
	};
	u32 gpio5c_iomux;
	u32 reserved5;
	union {
		u32 gpio6a_iomux;
		u32 iomux_i2s;
	};
	union {
		u32 gpio6b_iomux;
		u32 iomux_i2c2;
		u32 iomux_i2sclk;
	};
	union {
		u32 gpio6c_iomux;
		u32 iomux_sdmmc0;
	};
	u32 reserved6;
	union {
		u32 gpio7a_iomux;
		u32 iomux_pwm0;
		u32 iomux_pwm1;
	};
	union {
		u32 gpio7b_iomux;
		u32 iomux_edp_hotplug;
	};
	union {
		u32 gpio7cl_iomux;
		u32 iomux_i2c5sda;
		u32 iomux_i2c4;
	};
	union {
		u32 gpio7ch_iomux;
		u32 iomux_uart2;
		u32 iomux_i2c5scl;
	};
	u32 reserved7;
	union {
		u32 gpio8a_iomux;
		u32 iomux_spi2csclk;
		u32 iomux_i2c1;
	};
	union {
		u32 gpio8b_iomux;
		u32 iomux_spi2txrx;
	};
	u32 reserved8[30];
	struct rk3288_grf_gpio_lh gpio_sr[8];
	u32 gpio1_p[8][4];
	u32 gpio1_e[8][4];
	u32 gpio_smt;
	u32 soc_con0;
	u32 soc_con1;
	u32 soc_con2;
	u32 soc_con3;
	u32 soc_con4;
	u32 soc_con5;
	u32 soc_con6;
	u32 soc_con7;
	u32 soc_con8;
	u32 soc_con9;
	u32 soc_con10;
	u32 soc_con11;
	u32 soc_con12;
	u32 soc_con13;
	u32 soc_con14;
	u32 soc_status[22];
	u32 reserved9[2];
	u32 peridmac_con[4];
	u32 ddrc0_con0;
	u32 ddrc1_con0;
	u32 cpu_con[5];
	u32 reserved10[3];
	u32 cpu_status0;
	u32 reserved11;
	u32 uoc0_con[5];
	u32 uoc1_con[5];
	u32 uoc2_con[4];
	u32 uoc3_con[2];
	u32 uoc4_con[2];
	u32 pvtm_con[3];
	u32 pvtm_status[3];
	u32 io_vsel;
	u32 saradc_testbit;
	u32 tsadc_testbit_l;
	u32 tsadc_testbit_h;
	u32 os_reg[4];
	u32 reserved12;
	u32 soc_con15;
	u32 soc_con16;
};

struct rk3288_sgrf_regs {
	u32 soc_con0;
	u32 soc_con1;
	u32 soc_con2;
	u32 soc_con3;
	u32 soc_con4;
	u32 soc_con5;
	u32 reserved1[(0x20-0x18)/4];
	u32 busdmac_con[2];
	u32 reserved2[(0x40-0x28)/4];
	u32 cpu_con[3];
	u32 reserved3[(0x50-0x4c)/4];
	u32 soc_con6;
	u32 soc_con7;
	u32 soc_con8;
	u32 soc_con9;
	u32 soc_con10;
	u32 soc_con11;
	u32 soc_con12;
	u32 soc_con13;
	u32 soc_con14;
	u32 soc_con15;
	u32 soc_con16;
	u32 soc_con17;
	u32 soc_con18;
	u32 soc_con19;
	u32 soc_con20;
	u32 soc_con21;
	u32 reserved4[(0x100-0x90)/4];
	u32 soc_status[2];
	u32 reserved5[(0x120-0x108)/4];
	u32 fast_boot_addr;
};

#endif
