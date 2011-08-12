/*
 * (C) Copyright 2011
 * DENX Software Engineering, Anatolij Gustschin <agust@denx.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <common.h>
#include <asm/fsl_law.h>
#include <asm/immap_85xx.h>

enum law_size msize_to_law_size(long size)
{
	switch (size) {
	case 0x40000000:
		return LAW_SIZE_1G;
	case 0x20000000:
		return LAW_SIZE_512M;
	case 0x10000000:
		return LAW_SIZE_256M;
	case 0x08000000:
		return LAW_SIZE_128M;
	case 0x04000000:
		return LAW_SIZE_64M;
	case 0x02000000:
		return LAW_SIZE_32M;
	case 0x01000000:
		return LAW_SIZE_16M;
	case 0x00800000:
		return LAW_SIZE_8M;
	case 0x00400000:
		return LAW_SIZE_4M;
	case 0x00200000:
		return LAW_SIZE_2M;
	case 0x00100000:
		return LAW_SIZE_1M;
	case 0x00080000:
		return LAW_SIZE_512K;
	case 0x00040000:
		return LAW_SIZE_256K;
	case 0x00020000:
		return LAW_SIZE_128K;
	case 0x00010000:
		return LAW_SIZE_64K;
	case 0x00008000:
		return LAW_SIZE_32K;
	case 0x00004000:
		return LAW_SIZE_16K;
	case 0x00002000:
		return LAW_SIZE_8K;
	case 0x00001000:
		return LAW_SIZE_4K;
	default:
		return 0;
	}
}

/* Fixed sdram init */
phys_size_t fixed_sdram(void)
{
#ifndef CONFIG_SYS_RAMBOOT
	ccsr_ddr_t *ddr = (ccsr_ddr_t *)CONFIG_SYS_MPC85xx_DDR_ADDR;
	struct law_entry entry;

	set_next_law(CONFIG_SYS_DDR_SDRAM_BASE, LAW_SIZE_2G, LAW_TRGT_IF_DDR_1);

	out_be32(&ddr->cs0_bnds, CONFIG_SYS_DDR_CS0_BNDS);
	out_be32(&ddr->cs0_config, CONFIG_SYS_DDR_CS0_CONFIG);
	out_be32(&ddr->cs1_bnds, CONFIG_SYS_DDR_CS1_BNDS);
	out_be32(&ddr->cs1_config, CONFIG_SYS_DDR_CS1_CONFIG);
	out_be32(&ddr->timing_cfg_3, CONFIG_SYS_DDR_TIMING_3);
	out_be32(&ddr->timing_cfg_0, CONFIG_SYS_DDR_TIMING_0);
	out_be32(&ddr->timing_cfg_1, CONFIG_SYS_DDR_TIMING_1);
	out_be32(&ddr->timing_cfg_2, CONFIG_SYS_DDR_TIMING_2);

	out_be32(&ddr->sdram_cfg, CONFIG_SYS_DDR_CONTROL);
	out_be32(&ddr->sdram_cfg_2, CONFIG_SYS_DDR_CONTROL2);
	out_be32(&ddr->sdram_mode, CONFIG_SYS_DDR_MODE_1);
	out_be32(&ddr->sdram_mode_2, CONFIG_SYS_DDR_MODE_2);
	out_be32(&ddr->sdram_interval, CONFIG_SYS_DDR_INTERVAL);
	out_be32(&ddr->sdram_data_init, CONFIG_SYS_DDR_DATA_INIT);
	out_be32(&ddr->sdram_clk_cntl, CONFIG_SYS_DDR_CLK_CTRL);
	out_be32(&ddr->timing_cfg_4, CONFIG_SYS_DDR_TIMING_4);
	out_be32(&ddr->timing_cfg_5, CONFIG_SYS_DDR_TIMING_5);
	out_be32(&ddr->ddr_zq_cntl, CONFIG_SYS_DDR_ZQ_CNTL);
	out_be32(&ddr->ddr_wrlvl_cntl, CONFIG_SYS_DDR_WRLVL_CNTL);
	out_be32(&ddr->ddr_cdr1, CONFIG_SYS_DDR_CDR_1);
	out_be32(&ddr->ddr_cdr2, CONFIG_SYS_DDR_CDR_2);
	udelay(1000);
	out_be32(&ddr->sdram_cfg, CONFIG_SYS_DDR_CONTROL | 0x80000000);
	udelay(1000);

	entry = find_law(CONFIG_SYS_DDR_SDRAM_BASE);
	set_law(entry.index, CONFIG_SYS_DDR_SDRAM_BASE,
		msize_to_law_size(CONFIG_SYS_SDRAM_SIZE), LAW_TRGT_IF_DDR_1);
#endif
	return CONFIG_SYS_SDRAM_SIZE;
}
