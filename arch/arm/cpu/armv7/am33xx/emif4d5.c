/*
 * emif45.c
 *
 * AM43XX emif4d5 configuration file
 *
 * Copyright (C) 2013, Texas Instruments, Incorporated - http://www.ti.com/
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <asm/arch/cpu.h>
#include <asm/arch/ddr_defs.h>
#include <asm/arch/hardware.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>
#include <asm/io.h>
#include <asm/emif.h>

static struct ddr_ctrl *ddrctrl = (struct ddr_ctrl *)DDR_CTRL_ADDR;
static struct cm_device_inst *cm_device =
				(struct cm_device_inst *)CM_DEVICE_INST;
static struct ddr_cmdtctrl *ioctrl_reg =
			(struct ddr_cmdtctrl *)DDR_CONTROL_BASE_ADDR;
static struct vtp_reg *vtpreg = (struct vtp_reg *)VTP0_CTRL_ADDR;

DECLARE_GLOBAL_DATA_PTR;

int dram_init(void)
{
	/* dram_init must store complete ramsize in gd->ram_size */
	gd->ram_size = get_ram_size(
			(void *)CONFIG_SYS_SDRAM_BASE,
			CONFIG_MAX_RAM_BANK_SIZE);
	return 0;
}

void dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = CONFIG_SYS_SDRAM_BASE;
	gd->bd->bi_dram[0].size = gd->ram_size;
}

static void config_vtp(void)
{
	writel(readl(&vtpreg->vtp0ctrlreg) | VTP_CTRL_ENABLE,
			&vtpreg->vtp0ctrlreg);
	writel(readl(&vtpreg->vtp0ctrlreg) & (~VTP_CTRL_START_EN),
			&vtpreg->vtp0ctrlreg);
	writel(readl(&vtpreg->vtp0ctrlreg) | VTP_CTRL_START_EN,
			&vtpreg->vtp0ctrlreg);

	while ((readl(&vtpreg->vtp0ctrlreg) & VTP_CTRL_READY) !=
			VTP_CTRL_READY)
		;
}

static inline u32 get_mr(u32 base, u32 cs, u32 mr_addr)
{
	u32 mr;
	struct emif_reg_struct *emif = (struct emif_reg_struct *)base;

	mr_addr |= cs << EMIF_REG_CS_SHIFT;
	writel(mr_addr, &emif->emif_lpddr2_mode_reg_cfg);
	
	mr = readl(&emif->emif_lpddr2_mode_reg_data);
	debug("get_mr: EMIF1 cs %d mr %08x val 0x%x\n", cs, mr_addr, mr);
	if (((mr & 0x0000ff00) >>  8) == (mr & 0xff) &&
	    ((mr & 0x00ff0000) >> 16) == (mr & 0xff) &&
	    ((mr & 0xff000000) >> 24) == (mr & 0xff))
		return mr & 0xff;
	else
		return mr;
}

static inline void set_mr(u32 base, u32 cs, u32 mr_addr, u32 mr_val)
{
	struct emif_reg_struct *emif = (struct emif_reg_struct *)base;

	mr_addr |= cs << EMIF_REG_CS_SHIFT;
	writel(mr_addr, &emif->emif_lpddr2_mode_reg_cfg);
	writel(mr_val, &emif->emif_lpddr2_mode_reg_data);
}

static void configure_mr(u32 base, u32 cs)
{
	int i;
	struct emif_reg_struct *emif = (struct emif_reg_struct *)EMIF1_BASE;
	u32 mr_addr;

	while (get_mr(base, cs, LPDDR2_MR0) & LPDDR2_MR0_DAI_MASK)
		;
	set_mr(base, cs, LPDDR2_MR10, 0x56);
	for (i=0;i<2000;i++)
		readl(&emif->emif_pwr_mgmt_ctrl);

	set_mr(base, cs, LPDDR2_MR1, 0x43);
	set_mr(base, cs, LPDDR2_MR2, 0x2);

	mr_addr = LPDDR2_MR2 | EMIF_REG_REFRESH_EN_MASK;
	set_mr(base, cs, mr_addr, 0x2);
}

void do_sdram_init(const struct emif_regs *regs)
{
	struct emif_reg_struct *emif = (struct emif_reg_struct *)EMIF1_BASE;
	u32 i = 0;

	config_vtp();

	writel(readl(&cm_device->cm_dll_ctrl) & ~0x1, &cm_device->cm_dll_ctrl);
	while((readl(&cm_device->cm_dll_ctrl) && CM_DLL_READYST) == 0);

	/* io settings */
	writel(LPDDR2_ADDRCTRL_IOCTRL_VALUE, &ioctrl_reg->cm0ioctl);
	writel(LPDDR2_ADDRCTRL_WD0_IOCTRL_VALUE, &ioctrl_reg->cm1ioctl);
	writel(LPDDR2_ADDRCTRL_WD1_IOCTRL_VALUE, &ioctrl_reg->cm2ioctl);
	writel(LPDDR2_DATA0_IOCTRL_VALUE, &ioctrl_reg->dt0ioctl);
	writel(LPDDR2_DATA1_IOCTRL_VALUE, &ioctrl_reg->dt1ioctl);
	writel(LPDDR2_DATA2_IOCTRL_VALUE, &ioctrl_reg->dt2ioctrl);
	writel(LPDDR2_DATA3_IOCTRL_VALUE, &ioctrl_reg->dt3ioctrl);

	writel(0x80003000, &emif->emif_sdram_ref_ctrl);

	writel(0x1, &ioctrl_reg->emif_sdram_config_ext);

	/* Set CKE to be controlled by EMIF/DDR PHY */
	writel(readl(&ddrctrl->ddrckectrl) | 0x3, &ddrctrl->ddrckectrl);

	writel(regs->sdram_tim1, &emif->emif_sdram_tim_1);
	writel(regs->sdram_tim1, &emif->emif_sdram_tim_1_shdw);
	writel(regs->sdram_tim2, &emif->emif_sdram_tim_2);
	writel(regs->sdram_tim2, &emif->emif_sdram_tim_2_shdw);
	writel(regs->sdram_tim3, &emif->emif_sdram_tim_3);
	writel(regs->sdram_tim3, &emif->emif_sdram_tim_3_shdw);

	writel(0x0, &emif->emif_pwr_mgmt_ctrl);
	writel(0x0, &emif->emif_pwr_mgmt_ctrl_shdw);
	writel(regs->zq_config, &emif->emif_zq_config);
	writel(regs->temp_alert_config, &emif->emif_temp_alert_config);
	writel(regs->emif_ddr_phy_ctlr_1, &emif->emif_ddr_phy_ctrl_1);
	writel(regs->emif_ddr_phy_ctlr_1, &emif->emif_ddr_phy_ctrl_1_shdw);
	writel(0x0A000000, &emif->emif_l3_config);

	writel(0x04010040, &emif->emif_ddr_ext_phy_ctrl_1);
	writel(0x04010040, &emif->emif_ddr_ext_phy_ctrl_1_shdw);
	writel(0x00500050, &emif->emif_ddr_ext_phy_ctrl_2);
	writel(0x00500050, &emif->emif_ddr_ext_phy_ctrl_2_shdw);
	writel(0x00500050, &emif->emif_ddr_ext_phy_ctrl_3);
	writel(0x00500050, &emif->emif_ddr_ext_phy_ctrl_3_shdw);
	writel(0x00500050, &emif->emif_ddr_ext_phy_ctrl_4);
	writel(0x00500050, &emif->emif_ddr_ext_phy_ctrl_4_shdw);
	writel(0x00500050, &emif->emif_ddr_ext_phy_ctrl_5);
	writel(0x00500050, &emif->emif_ddr_ext_phy_ctrl_5_shdw);
	writel(0x00500050, &emif->emif_ddr_ext_phy_ctrl_6);
	writel(0x00500050, &emif->emif_ddr_ext_phy_ctrl_6_shdw);
	writel(0x00350035, &emif->emif_ddr_ext_phy_ctrl_7);
	writel(0x00350035, &emif->emif_ddr_ext_phy_ctrl_7_shdw);
	writel(0x00350035, &emif->emif_ddr_ext_phy_ctrl_8);
	writel(0x00350035, &emif->emif_ddr_ext_phy_ctrl_8_shdw);
	writel(0x00350035, &emif->emif_ddr_ext_phy_ctrl_9);
	writel(0x00350035, &emif->emif_ddr_ext_phy_ctrl_9_shdw);
	writel(0x00350035, &emif->emif_ddr_ext_phy_ctrl_10);
	writel(0x00350035, &emif->emif_ddr_ext_phy_ctrl_10_shdw);
	writel(0x00350035, &emif->emif_ddr_ext_phy_ctrl_11);
	writel(0x00350035, &emif->emif_ddr_ext_phy_ctrl_11_shdw);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_12);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_12_shdw);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_13);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_13_shdw);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_14);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_14_shdw);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_15);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_15_shdw);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_16);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_16_shdw);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_17);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_17_shdw);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_18);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_18_shdw);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_19);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_19_shdw);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_20);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_20_shdw);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_21);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_21_shdw);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_22);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_22_shdw);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_23);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_23_shdw);
	writel(0x40001000, &emif->emif_ddr_ext_phy_ctrl_24);
	writel(0x40001000, &emif->emif_ddr_ext_phy_ctrl_24_shdw);
	writel(0x08102040, &emif->emif_ddr_ext_phy_ctrl_25);
	writel(0x08102040, &emif->emif_ddr_ext_phy_ctrl_25_shdw);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_26);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_26_shdw);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_27);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_27_shdw);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_28);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_28_shdw);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_29);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_29_shdw);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_30);
	writel(0x0, &emif->emif_ddr_ext_phy_ctrl_30_shdw);

	writel(0x3000, &emif->emif_sdram_ref_ctrl);
	
	writel(regs->sdram_config, &emif->emif_sdram_config);
	writel(0x40d, &emif->emif_sdram_ref_ctrl);

	for (i=0;i<700;i++) {
		readl(&emif->emif_pwr_mgmt_ctrl);
	}

	configure_mr(EMIF1_BASE, 0);
	configure_mr(EMIF1_BASE, 1);
}
