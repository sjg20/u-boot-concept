/*
 * board.c
 *
 * Board functions for TI AM43XX based boards
 *
 * Copyright (C) 2013, Texas Instruments, Incorporated - http://www.ti.com/
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <i2c.h>
#include <spl.h>
#include <asm/arch/clock.h>
#include <asm/arch/cpu.h>
#include <asm/arch/hardware.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/mux.h>
#include <asm/arch/ddr_defs.h>
#include <asm/emif.h>
#include <asm/errno.h>
#include "board.h"
#include <miiphy.h>
#include <cpsw.h>
#include "emif.h"

DECLARE_GLOBAL_DATA_PTR;
#define WR_MEM_32(addr, data) *(unsigned int*)(addr) = (unsigned int)(data)
#define RD_MEM_32(addr)       *(unsigned int*)(addr)

/*
 * Read header information from EEPROM into global structure.
 */
static int read_eeprom(struct am43xx_board_id *header)
{
	/* Check if baseboard eeprom is available */
	if (i2c_probe(CONFIG_SYS_I2C_EEPROM_ADDR)) {
		printf("Could not probe the EEPROM at 0x%x; "
		     "something fundamentally wrong on the I2C bus.\n",
		     CONFIG_SYS_I2C_EEPROM_ADDR);
		return -ENODEV;
	}

	/* read the eeprom using i2c */
	if (i2c_read(CONFIG_SYS_I2C_EEPROM_ADDR, 0, 2, (uchar *)header,
		     sizeof(struct am43xx_board_id))) {
		puts("Could not read the EEPROM; something fundamentally"
			" wrong on the I2C bus.\n");
		return -EIO;
	}

	if (header->magic != 0xEE3355AA) {
		/*
		 * read the eeprom using i2c again,
		 * but use only a 1 byte address
		 */
		if (i2c_read(CONFIG_SYS_I2C_EEPROM_ADDR, 0, 1, (uchar *)header,
			     sizeof(struct am43xx_board_id))) {
			printf("Could not read the EEPROM at 0x%x; something "
			     "fundamentally wrong on the I2C bus.\n",
			     CONFIG_SYS_I2C_EEPROM_ADDR);
			return -EIO;
		}

		if (header->magic != 0xEE3355AA) {
			printf("Incorrect magic number (0x%x) in EEPROM\n",
					header->magic);
			return -EINVAL;
		}
	}

	return 0;
}

#ifdef CONFIG_SPL_BUILD
const struct dpll_params dpll_ddr = {
		333, 23, 1, -1, 1, -1, -1};

const struct emif_regs eposevm_emif_regs = {
	//.sdram_config_init		= 0x80800EBA,
	.sdram_config			= 0x808012BA,
	.ref_ctrl			= 0x0000040D,
	.sdram_tim1			= 0xEA86B412,
	.sdram_tim2			= 0x1025094A,
	.sdram_tim3			= 0x0F6BA22F,
	.read_idle_ctrl			= 0x00050000,
	.zq_config			= 0xD00fFFFF,
	.temp_alert_config		= 0x0,
	//.emif_ddr_phy_ctlr_1_init	= 0x0E28420d,
	.emif_ddr_phy_ctlr_1		= 0x0E084006,
	.emif_ddr_ext_phy_ctrl_1	= 0x04010040,
	.emif_ddr_ext_phy_ctrl_2	= 0x00500050,
	.emif_ddr_ext_phy_ctrl_3	= 0x00500050,
	.emif_ddr_ext_phy_ctrl_4	= 0x00500050,
	.emif_ddr_ext_phy_ctrl_5	= 0x00500050
};

const struct dpll_params *get_dpll_ddr_params(void)
{
	return &dpll_ddr;
}

void set_uart_mux_conf(void)
{
	enable_uart0_pin_mux();
}

void set_mux_conf_regs(void)
{
	enable_board_pin_mux();
}

#define  DDR_ADDRCTRL_WD0_IOCTRL_VALUE 0x00000000
#define  DDR_ADDRCTRL_WD1_IOCTRL_VALUE 0x00000000
//#define  DDR_DATA0_IOCTRL_VALUE   0x294
//#define  DDR_DATA1_IOCTRL_VALUE   0x294
//#define  DDR_DATA2_IOCTRL_VALUE   0x294
//#define  DDR_DATA3_IOCTRL_VALUE   0x294
#define  DDR_ADDRCTRL_IOCTRL_VALUE   0x84 //0x294   //e7
#define  DDR_DATA0_IOCTRL_VALUE   0x84  //0xE7  //84
#define  DDR_DATA1_IOCTRL_VALUE   0x84  //0xe7 //84
#define  DDR_DATA2_IOCTRL_VALUE   0x84  //e7 //84
#define  DDR_DATA3_IOCTRL_VALUE   0x84  //e7 //84

//#define ALLOPP_DDR3_READ_LATENCY    0x07        //RD_Latency = (CL + 2) - 1
#define ALLOPP_DDR3_SDRAM_TIMING1   0xE888B41B
#define ALLOPP_DDR3_SDRAM_TIMING2   0x26597FDA
#define ALLOPP_DDR3_SDRAM_TIMING3   0x17F85688

//#define ALLOPP_DDR3_SDRAM_CONFIG      0x638453B2  //16-bit
#define ALLOPP_DDR3_SDRAM_CONFIG      0x638413B2  //32-bit


#define ALLOPP_DDR3_REF_CTRL        0x00000A25  //333 * 7.8us = 0xA25
#define ALLOPP_DDR3_ZQ_CONFIG       0x50074BE4

void Enable_VTT_Regulator(void)
{
  u32 temp;

    /*GPIO_VTTEN - GPIO0_21 PINMUX Setup*/
    WR_MEM_32(CONTROL_CONF_SPI2_SCLK, 0x20009);

    WR_MEM_32(CM_WKUP_GPIO0_CLKCTRL,0x40002);
    /* Poll if module is functional */
    while((RD_MEM_32(CM_WKUP_GPIO0_CLKCTRL) & 0x30000) != 0x0);


    while( (RD_MEM_32(CM_WKUP_CLKSTCTRL) & 0x100) != 0x100);

    //reset the GPIO module
//    WR_MEM_32(GPIO0_SYSCONFIG,0x2);
//    while(RD_MEM_32(GPIO0_SYSSTATUS)!= 0x1);
//    GEL_TextOut("GPIO0 reset \n","Output",1,1,1);

    //enable module
    WR_MEM_32(GPIO0_CTRL,0x0);

    /*enable output for GPIO0_21*/
     WR_MEM_32((GPIO0_SETDATAOUT),(1<<22));
     temp = RD_MEM_32(GPIO0_OE);
     temp = temp & ~(1 << 22);
     WR_MEM_32(GPIO0_OE,temp);

}

void VTP_Enable(void)
{
   WR_MEM_32(VTP_CTRL_REG ,(RD_MEM_32(VTP_CTRL_REG) | 0x00000040));
   //Write 0 to CLRZ bit
   WR_MEM_32(VTP_CTRL_REG ,(RD_MEM_32(VTP_CTRL_REG) & 0xFFFFFFFE));
   //Write 1 to CLRZ bit
   WR_MEM_32(VTP_CTRL_REG ,(RD_MEM_32(VTP_CTRL_REG) | 0x00000001));
   //Check for VTP ready bit
   //while((RD_MEM_32(VTP_CTRL_REG) & 0x00000020) != 0x00000020);
}

void AM43xx_DDR3_config_SW_lvl(void)
{
	int i, read_reg;

	VTP_Enable();

     WR_MEM_32(CM_DLL_CTRL, RD_MEM_32(CM_DLL_CTRL) & ~0x00000001 );

     //wait for DLL ready
     while((RD_MEM_32(CM_DLL_CTRL) & 0x4) == 0);

          WR_MEM_32(DDR_ADDRCTRL_IOCTRL,DDR_ADDRCTRL_IOCTRL_VALUE);
          //WR_MEM_32(DDR_ADDRCTRL_WD0_IOCTRL,DDR_ADDRCTRL_WD0_IOCTRL_VALUE);
          //WR_MEM_32(DDR_ADDRCTRL_WD1_IOCTRL,DDR_ADDRCTRL_WD1_IOCTRL_VALUE);
          WR_MEM_32(DDR_DATA0_IOCTRL,DDR_DATA0_IOCTRL_VALUE);
          WR_MEM_32(DDR_DATA1_IOCTRL,DDR_DATA1_IOCTRL_VALUE);
          WR_MEM_32(DDR_DATA2_IOCTRL,DDR_DATA2_IOCTRL_VALUE);
          WR_MEM_32(DDR_DATA3_IOCTRL,DDR_DATA3_IOCTRL_VALUE);

// EMIF  PHYs extra CONFIG - CTRL_MODULE_WKUP

          //WR_MEM_32(EMIF_SDRAM_CONFIG_EXT,0x00067);
          //WR_MEM_32(EMIF_SDRAM_CONFIG_EXT,0x20043);    //for narrow mode (16-bit)
          WR_MEM_32(EMIF_SDRAM_CONFIG_EXT,0x0043);    //for 32-bit

          //WR_MEM_32(EMIF_SDRAM_CONFIG_EXT,0x20003);    //for narrow mode
	  //WR_MEM_32(EMIF_SDRAM_CONFIG_EXT,0x20003);    //for narrow mode
          //WR_MEM_32(EMIF_SDRAM_CONFIG_EXT,0x20063);    //for narrow mode


 // Allow EMIFC/PHY to drive the DDR3 RESET

          WR_MEM_32(DDR_IO_CTRL, 0x0);

//CKE controlled by EMIF/DDR_PHY
                  WR_MEM_32(DDR_CKE_CTRL, 0x3);

// EMIF1 controller CONFIG
	/**** 333MHz ******/

    WR_MEM_32(EMIF_SDRAM_TIM_1,ALLOPP_DDR3_SDRAM_TIMING1);
    WR_MEM_32(EMIF_SDRAM_TIM_1_SHDW,ALLOPP_DDR3_SDRAM_TIMING1);

    WR_MEM_32(EMIF_SDRAM_TIM_2,ALLOPP_DDR3_SDRAM_TIMING2);
    WR_MEM_32(EMIF_SDRAM_TIM_2_SHDW,ALLOPP_DDR3_SDRAM_TIMING2);

    WR_MEM_32(EMIF_SDRAM_TIM_3,ALLOPP_DDR3_SDRAM_TIMING3);
    WR_MEM_32(EMIF_SDRAM_TIM_3_SHDW,ALLOPP_DDR3_SDRAM_TIMING3);

	*(int*)(0x4C000030)= 0x00000000; //LPDDR2_NVM_TIM --
	*(int*)(0x4C000034)= 0x00000000; //LPDDR2_NVM_TIM_SHDW --
	*(int*)(0x4C000038)= 0x00000000; //PWR_MGMT_CTRL --
	*(int*)(0x4C00003C)= 0x00000000; //PWR_MGMT_CTRL_SHDW --
	*(int*)(0x4C000054)= 0x0A500000; //OCP_CONFIG --
	*(int*)(0x4C000060)= 0x00000001; //IODFT_TLGC --
	//*(int*)(0x4C000098)= 0x00050000; //DLL_CALIB_CTRL --
	//*(int*)(0x4C00009C)= 0x00050000; //DLL_CALIB_CTRL_SHDW
	//*(int*)(0x4C0000C8)= 0xd00fffff; //ZQ_CONFIG --
	*(int*)(0x4C0000C8)= 0x0007190b; //ZQ_CONFIG --

	*(int*)(0x4C0000CC)= 0x00000000; //TEMP_ALERT_CONFIG
	*(int*)(0x4C0000D4)= 0x00000000; //RDWR_LVL_RMP_WIN --
	*(int*)(0x4C0000D8)= 0x00000000; //RDWR_LVL_RMP_CTRL --
	*(int*)(0x4C0000DC)= 0x00000000; //RDWR_LVL_CTRL --
	*(int*)(0x4C0000E4)= 0x0E004007; //DDR_PHY_CTRL_1 -- force invert_clkout=0 for now
	*(int*)(0x4C0000E8)= 0x0E004007; //DDR_PHY_CTRL_1_SHDW -- force invert_clkout=0 for now
	//*(int*)(0x4C0000EC)= 0x00000000; //DDR_PHY_CTRL_2 --
	*(int*)(0x4C000100)= 0x00000000; //PRI_COS_MAP --
	*(int*)(0x4C000104)= 0x00000000; //CONNID_COS_1_MAP --
	*(int*)(0x4C000108)= 0x00000000; //CONNID_COS_2_MAP --
	*(int*)(0x4C000120)= 0x00000405; //RD_WR_EXEC_THRSH --
	*(int*)(0x4C000124)= 0x00FFFFFF; //COS_CONFIG --

//reg_phy_ctrl_slave_ratio
	*(int*)(0x4C000200)= 0x08020080; //EXT_PHY_CTRL_1 --   10bits wide each slice, 3 slices concatenated
	*(int*)(0x4C000204)= 0x08020080; //EXT_PHY_CTRL_1_SHDW--
//	*(int*)(0x4C000200)= 0x04020080; //EXT_PHY_CTRL_1 --   10bits wide each slice, 3 slices concatenated
//	*(int*)(0x4C000204)= 0x04020080; //EXT_PHY_CTRL_1_SHDW--
//	*(int*)(0x4C000200)= 0x10040100; //EXT_PHY_CTRL_1 --   10bits wide each slice, 3 slices concatenated
//	*(int*)(0x4C000204)= 0x10040100; //EXT_PHY_CTRL_1_SHDW--

//#define PHY_FIFO_WE_SLAVE_RATIO 0x00200020	  //RD Gate
//#define PHY_FIFO_WE_SLAVE_RATIO 0x00af00af
//#define PHY_FIFO_WE_SLAVE_RATIO 0x00400040
//#define PHY_FIFO_WE_SLAVE_RATIO 0x02000200
//#define PHY_FIFO_WE_SLAVE_RATIO 0x00d700d7
//#define PHY_FIFO_WE_SLAVE_RATIO 0x01100110
#define PHY_FIFO_WE_SLAVE_RATIO 0x00ad00ad

	//reg_phy_fifo_we_slave_ratio -- Need to change as per the board delay   TRM definition is correct
	*(int*)(0x4C000208)= PHY_FIFO_WE_SLAVE_RATIO; //EXT_PHY_CTRL_2 --
	*(int*)(0x4C00020C)= PHY_FIFO_WE_SLAVE_RATIO; //EXT_PHY_CTRL_2_SHDW--
	*(int*)(0x4C000210)= PHY_FIFO_WE_SLAVE_RATIO; //EXT_PHY_CTRL_3 --
	*(int*)(0x4C000214)= PHY_FIFO_WE_SLAVE_RATIO; //EXT_PHY_CTRL_3_SHDW--
	*(int*)(0x4C000218)= PHY_FIFO_WE_SLAVE_RATIO; //EXT_PHY_CTRL_4 --
	*(int*)(0x4C00021C)= PHY_FIFO_WE_SLAVE_RATIO; //EXT_PHY_CTRL_4_SHDW --
	*(int*)(0x4C000220)= PHY_FIFO_WE_SLAVE_RATIO; //EXT_PHY_CTRL_5 --
	*(int*)(0x4C000224)= PHY_FIFO_WE_SLAVE_RATIO; //EXT_PHY_CTRL_5_SHDW --
	*(int*)(0x4C000228)= PHY_FIFO_WE_SLAVE_RATIO; //EXT_PHY_CTRL_6 --
	*(int*)(0x4C00022C)= PHY_FIFO_WE_SLAVE_RATIO; //EXT_PHY_CTRL_6_SHDW --

//#define PHY_RD_DQS_SLAVE_RATIO 0x00400040
//#define PHY_RD_DQS_SLAVE_RATIO 0x00200020
//#define PHY_RD_DQS_SLAVE_RATIO 0x00800080
#define PHY_RD_DQS_SLAVE_RATIO 0x00350035
//#define PHY_RD_DQS_SLAVE_RATIO 0x00100010
//#define PHY_RD_DQS_SLAVE_RATIO 0x00250025

	//reg_phy_rd_dqs_slave_ratio
	*(int*)(0x4C000230)= PHY_RD_DQS_SLAVE_RATIO; //EXT_PHY_CTRL_7 --
	*(int*)(0x4C000234)= PHY_RD_DQS_SLAVE_RATIO; //EXT_PHY_CTRL_7_SHDW --
	*(int*)(0x4C000238)= PHY_RD_DQS_SLAVE_RATIO; //EXT_PHY_CTRL_8 --
	*(int*)(0x4C00023C)= PHY_RD_DQS_SLAVE_RATIO; //EXT_PHY_CTRL_8_SHDW --
	*(int*)(0x4C000240)= PHY_RD_DQS_SLAVE_RATIO; //EXT_PHY_CTRL_9 --
	*(int*)(0x4C000244)= PHY_RD_DQS_SLAVE_RATIO; //EXT_PHY_CTRL_9_SHDW --
	*(int*)(0x4C000248)= PHY_RD_DQS_SLAVE_RATIO; //EXT_PHY_CTRL_10 --
	*(int*)(0x4C00024C)= PHY_RD_DQS_SLAVE_RATIO; //EXT_PHY_CTRL_10_SHDW --
	*(int*)(0x4C000250)= PHY_RD_DQS_SLAVE_RATIO; //EXT_PHY_CTRL_11 --
	*(int*)(0x4C000254)= PHY_RD_DQS_SLAVE_RATIO; //EXT_PHY_CTRL_11_SHDW --

//#define PHY_WR_DATA_SLAVE_RATIO 0x00400040
//#define PHY_WR_DATA_SLAVE_RATIO 0x00830083
//#define PHY_WR_DATA_SLAVE_RATIO 0x00500050
//#define PHY_WR_DATA_SLAVE_RATIO 0x01000100
#define PHY_WR_DATA_SLAVE_RATIO 0x00000000

	//reg_phy_wr_data_slave_ratio
	*(int*)(0x4C000258)= PHY_WR_DATA_SLAVE_RATIO; //EXT_PHY_CTRL_12 --
	*(int*)(0x4C00025C)= PHY_WR_DATA_SLAVE_RATIO; //EXT_PHY_CTRL_12_SHDW --
	*(int*)(0x4C000260)= PHY_WR_DATA_SLAVE_RATIO; //EXT_PHY_CTRL_13 --
	*(int*)(0x4C000264)= PHY_WR_DATA_SLAVE_RATIO; //EXT_PHY_CTRL_13_SHDW --
	*(int*)(0x4C000268)= PHY_WR_DATA_SLAVE_RATIO; //EXT_PHY_CTRL_14 --
	*(int*)(0x4C00026C)= PHY_WR_DATA_SLAVE_RATIO; //EXT_PHY_CTRL_14_SHDW --
	*(int*)(0x4C000270)= PHY_WR_DATA_SLAVE_RATIO; //EXT_PHY_CTRL_15 --
	*(int*)(0x4C000274)= PHY_WR_DATA_SLAVE_RATIO; //EXT_PHY_CTRL_15_SHDW --
	*(int*)(0x4C000278)= PHY_WR_DATA_SLAVE_RATIO; //EXT_PHY_CTRL_16 --
	*(int*)(0x4C00027C)= PHY_WR_DATA_SLAVE_RATIO; //EXT_PHY_CTRL_16_SHDW --

//#define PHY_WR_DQS_SLAVE_RATIO 0x00000000
#define PHY_WR_DQS_SLAVE_RATIO 0x00340034
//#define PHY_WR_DQS_SLAVE_RATIO 0x00c300c3
//#define PHY_WR_DQS_SLAVE_RATIO 0x00800080
//#define PHY_WR_DQS_SLAVE_RATIO 0x00b400b4
	//reg_phy_wr_dqs_slave_ratio
	*(int*)(0x4C430290)= PHY_WR_DQS_SLAVE_RATIO; //EXT_PHY_CTRL_17 --
	*(int*)(0x4C430284)= PHY_WR_DQS_SLAVE_RATIO; //EXT_PHY_CTRL_17_SHDW --
	*(int*)(0x4C430288)= PHY_WR_DQS_SLAVE_RATIO; //EXT_PHY_CTRL_18 --
	*(int*)(0x4C43028C)= PHY_WR_DQS_SLAVE_RATIO; //EXT_PHY_CTRL_18_SHDW --
	*(int*)(0x4C430290)= PHY_WR_DQS_SLAVE_RATIO; //EXT_PHY_CTRL_19 --
	*(int*)(0x4C430294)= PHY_WR_DQS_SLAVE_RATIO; //EXT_PHY_CTRL_19_SHDW --
	*(int*)(0x4C430298)= PHY_WR_DQS_SLAVE_RATIO; //EXT_PHY_CTRL_20 --
	*(int*)(0x4C43029C)= PHY_WR_DQS_SLAVE_RATIO; //EXT_PHY_CTRL_20_SHDW --
	*(int*)(0x4C4302A0)= PHY_WR_DQS_SLAVE_RATIO; //EXT_PHY_CTRL_21 --
	*(int*)(0x4C4302A4)= PHY_WR_DQS_SLAVE_RATIO; //EXT_PHY_CTRL_21_SHDW --

//reg_phy_fifo_we_in_delay    reg_phy_ctrl_slave_delay
	//*(int*)(0x4C0002A8)= 0x820019;          //EXT_PHY_CTRL_22 --
	//*(int*)(0x4C0002AC)= 0x820019;          //EXT_PHY_CTRL_22_SHDW --

//foreced delayes: reg_phy_wr_dqs_slave_delay    reg_phy_rd_dqs_slave_delay
	//*(int*)(0x4C0002B0)= 0x000010;   //EXT_PHY_CTRL_23 --
	//*(int*)(0x4C0002B4)= 0x000010;   //EXT_PHY_CTRL_23_SHDW --

//reg_phy_dq_offset_hi    reg_phy_gatelvl_init_mode    reg_phy_use_rank0_delays    reg_phy_wr_data_slave_delay
*(int*)(0x4C0002B8)= 0x40000000; //EXT_PHY_CTRL_24 --
*(int*)(0x4C0002BC)= 0x40000000; //EXT_PHY_CTRL_24_SHDW --
//	*(int*)(0x4C0002B8)= 0x00000000; //EXT_PHY_CTRL_24 --
//	*(int*)(0x4C0002BC)= 0x00000000; //EXT_PHY_CTRL_24_SHDW --

//reg_phy_dq_offset : duirng WR leveling
*(int*)(0x4C0002C0)= 0x8102040;  //EXT_PHY_CTRL_25 --
*(int*)(0x4C0002C4)= 0x8102040;  //EXT_PHY_CTRL_25_SHDW --
//		*(int*)(0x4C0002C0)= 0x0;  //EXT_PHY_CTRL_25 --
//		*(int*)(0x4C0002C4)= 0x0;  //EXT_PHY_CTRL_25_SHDW --


//reg_phy_gatelvl_init_ratio
	*(int*)(0x4C0002C8)= 0x0;        //EXT_PHY_CTRL_26 --
	*(int*)(0x4C0002CC)= 0x0;        //EXT_PHY_CTRL_26_SHDW --
	*(int*)(0x4C0002D0)= 0x0;        //EXT_PHY_CTRL_27 --
	*(int*)(0x4C0002D4)= 0x0;        //EXT_PHY_CTRL_27_SHDW --
	*(int*)(0x4C0002D8)= 0x0;        //EXT_PHY_CTRL_28 --
	*(int*)(0x4C0002DC)= 0x0;        //EXT_PHY_CTRL_28_SHDW --
	*(int*)(0x4C0002E0)= 0x0;        //EXT_PHY_CTRL_29 --
	*(int*)(0x4C0002E4)= 0x0;        //EXT_PHY_CTRL_29_SHDW --
	*(int*)(0x4C0002E8)= 0x0;        //EXT_PHY_CTRL_30 --
	*(int*)(0x4C0002EC)= 0x0;        //EXT_PHY_CTRL_30_SHDW --

//reg_phy_wrlvl_init_ratio
	*(int*)(0x4C0002F0)= 0x0;        //EXT_PHY_CTRL_31 --
	*(int*)(0x4C0002F4)= 0x0;        //EXT_PHY_CTRL_31_SHDW --
	*(int*)(0x4C0002F8)= 0x0;        //EXT_PHY_CTRL_32 --
	*(int*)(0x4C0002FC)= 0x0;        //EXT_PHY_CTRL_32_SHDW --
	*(int*)(0x4C000300)= 0x0;        //EXT_PHY_CTRL_33 --
	*(int*)(0x4C000304)= 0x0;        //EXT_PHY_CTRL_33_SHDW --
	*(int*)(0x4C000308)= 0x0;        //EXT_PHY_CTRL_34 --
	*(int*)(0x4C00030C)= 0x0;        //EXT_PHY_CTRL_34_SHDW --
	*(int*)(0x4C000310)= 0x0;        //EXT_PHY_CTRL_35 --
	*(int*)(0x4C000314)= 0x0;        //EXT_PHY_CTRL_35_SHDW --

//reg_phy_wrlvl_num_of_dq0    reg_phy_gatelvl_num_of_dq0 misalign clean dll lock clear etc
	*(int*)(0x4C000318)= 0x0;        //EXT_PHY_CTRL_36 --
	*(int*)(0x4C00031C)= 0x0;        //EXT_PHY_CTRL_36_SHDW --

    //reset DDR PHY
//    WR_MEM_32(EMIF_IODFT_TLGC,RD_MEM_32(EMIF_IODFT_TLGC) | 0x400);
//    while(RD_MEM_32(EMIF_IODFT_TLGC) & 0x400 == 1);  //wait for reset to clear
//	GEL_TextOut(">> DDR PHY has been reset\n");


	*(int*)(0x4C000014)= ALLOPP_DDR3_REF_CTRL; //SDRAM_REF_CTRL_SHDW --
	*(int*)(0x4C000010)= ALLOPP_DDR3_REF_CTRL; //SDRAM_REF_CTRL -- //// INITREF disable bit is set to zero

	//*(int*)(0x4C00000C)= 0x08000000; //SDRAM_CONFIG_2 --
	*(int*)(0x4C000008)= ALLOPP_DDR3_SDRAM_CONFIG; //SDRAM_CONFIG -- //41811622; //DDR2 DDR3=61851AB2

	//hang loose till initialization is complete
	   for (i=0;i<700;i++) {
	   read_reg = RD_MEM_32(DDR_CKE_CTRL);
	   }

	printf("\n\n>>>>>>>DDR3 configuration is complete!!!\n\n");
}

void sdram_init(void)
{
	struct am43xx_board_id header;

	i2c_init(CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);
	if (read_eeprom(&header) < 0)
		puts("Could not get board ID.\n");

	if (board_is_eposevm(&header))
		do_sdram_init(&eposevm_emif_regs);
	else {
		Enable_VTT_Regulator();
		AM43xx_DDR3_config_SW_lvl();
	}
}
#endif

int board_init(void)
{
	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;

	return 0;
}

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	char safe_string[HDR_NAME_LEN + 1];
	struct am43xx_board_id header;

	if (read_eeprom(&header) < 0)
		puts("Could not get board ID.\n");

	/* Now set variables based on the header. */
	strncpy(safe_string, (char *)header.name, sizeof(header.name));
	safe_string[sizeof(header.name)] = 0;
	setenv("board_name", safe_string);

	strncpy(safe_string, (char *)header.version, sizeof(header.version));
	safe_string[sizeof(header.version)] = 0;
	setenv("board_rev", safe_string);
#endif
	return 0;
}
#endif

#ifdef CONFIG_DRIVER_TI_CPSW
static void cpsw_control(int enabled)
{
	/* Additional controls can be added here */
	return;
}

static struct cpsw_slave_data cpsw_slaves[] = {
	{
		.slave_reg_ofs	= 0x208,
		.sliver_reg_ofs	= 0xd80,
		.phy_id		= 2,
	},
	{
		.slave_reg_ofs	= 0x308,
		.sliver_reg_ofs	= 0xdc0,
		.phy_id		= 3,
	},
};

static struct cpsw_platform_data cpsw_data = {
	.mdio_base		= CPSW_MDIO_BASE,
	.cpsw_base		= CPSW_BASE,
	.mdio_div		= 0xff,
	.channels		= 8,
	.cpdma_reg_ofs		= 0x800,
	.slaves			= 1,
	.slave_data		= cpsw_slaves,
	.ale_reg_ofs		= 0xd00,
	.ale_entries		= 1024,
	.host_port_reg_ofs	= 0x108,
	.hw_stats_reg_ofs	= 0x900,
	.bd_ram_ofs		= 0x2000,
	.mac_control		= (1 << 5),
	.control		= cpsw_control,
	.host_port_num		= 0,
	.version		= CPSW_CTRL_VERSION_2,
};

static struct ctrl_dev *cdev = (struct ctrl_dev *)CTRL_DEVICE_BASE;

int board_eth_init(bd_t *bis)
{
	int rv, n = 0;
	uint8_t mac_addr[6];
	uint32_t mac_hi, mac_lo;

	/* try reading mac address from efuse */
	mac_lo = readl(&cdev->macid0l);
	mac_hi = readl(&cdev->macid0h);
	mac_addr[0] = mac_hi & 0xFF;
	mac_addr[1] = (mac_hi & 0xFF00) >> 8;
	mac_addr[2] = (mac_hi & 0xFF0000) >> 16;
	mac_addr[3] = (mac_hi & 0xFF000000) >> 24;
	mac_addr[4] = mac_lo & 0xFF;
	mac_addr[5] = (mac_lo & 0xFF00) >> 8;

	if (!getenv("ethaddr")) {
		printf("<ethaddr> not set. Validating first E-fuse MAC\n");

		if (is_valid_ether_addr(mac_addr))
			eth_setenv_enetaddr("ethaddr", mac_addr);
	}

	writel(RMII_MODE_ENABLE | RMII_CHIPCKL_ENABLE, &cdev->miisel);
	cpsw_slaves[0].phy_if = cpsw_slaves[1].phy_if =
				PHY_INTERFACE_MODE_RMII;

	rv = cpsw_register(&cpsw_data);
	if (rv < 0)
		printf("Error %d registering CPSW switch\n", rv);
	else
		n += rv;

	return n;
}
#endif
