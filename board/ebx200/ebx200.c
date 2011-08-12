/*
 * (C) Copyright 2011
 * DENX Software Engineering, Anatolij Gustschin <agust@denx.de>
 *
 * Based on p1022ds board code,
 * Copyright 2010-2011 Freescale Semiconductor, Inc.
 * Authors: Srikanth Srinivasan <srikanth.srinivasan@freescale.com>
 *          Timur Tabi <timur@freescale.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <common.h>
#include <command.h>
#include <asm/processor.h>
#include <asm/mmu.h>
#include <asm/cache.h>
#include <asm/immap_85xx.h>
#include <asm/fsl_pci.h>
#include <asm/io.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <fsl_mdio.h>
#include <tsec.h>
#include <asm/fsl_law.h>
#include <netdev.h>
#include <hwconfig.h>

DECLARE_GLOBAL_DATA_PTR;

int board_early_init_f(void)
{
	ccsr_gur_t *gur = (void *)CONFIG_SYS_MPC85xx_GUTS_ADDR;

	/* Set the ETSEC2 pin muxing to enable USB. */
	clrsetbits_be32(&gur->pmuxcr2, MPC85xx_PMUXCR2_ETSECUSB_MASK,
			MPC85xx_PMUXCR2_USB);
	/* Read back the register to synchronize the write. */
	in_be32(&gur->pmuxcr);

	return 0;
}

int checkboard(void)
{
	puts("Board: EBX200");
#ifdef CONFIG_PHYS_64BIT
	puts(" (36-bit addrmap)");
#endif
	puts("\n");
	return 0;
}

int misc_init_r(void)
{
	return 0;
}

int board_early_init_r(void)
{
	const unsigned int flashbase = CONFIG_SYS_FLASH_BASE;
	const u8 flash_esel = find_tlb_idx((void *)flashbase, 1);
	const u8 flash_esel1 = find_tlb_idx((void *)flashbase  + 0x4000000, 1);

	/*
	 * Remap Boot flash region to caching-inhibited
	 * so that flash can be erased properly.
	 */

	/* Flush d-cache and invalidate i-cache of any FLASH data */
	flush_dcache();
	invalidate_icache();

	/* invalidate existing TLB entries for flash */
	disable_tlb(flash_esel);
	disable_tlb(flash_esel1);

	set_tlb(1, flashbase, CONFIG_SYS_FLASH_BASE_PHYS,
		MAS3_SX | MAS3_SW | MAS3_SR, MAS2_I | MAS2_G,
		0, flash_esel, BOOKE_PAGESZ_64M, 1);

	set_tlb(1, flashbase + 0x4000000,
		CONFIG_SYS_FLASH_BASE_PHYS + 0x4000000,
		MAS3_SX | MAS3_SW | MAS3_SR, MAS2_I | MAS2_G,
		0, flash_esel1, BOOKE_PAGESZ_64M, 1);

	return 0;
}

int last_stage_init(void)
{
	unsigned short reg;

	/* Change LEDx settings for the Vitesse PHY */
	miiphy_write(DEFAULT_MII_NAME, TSEC1_PHY_ADDR, 0x1F, 1);

	miiphy_read(DEFAULT_MII_NAME, TSEC1_PHY_ADDR, 0x11, &reg);
	reg |= 1 << 4;		/* enhanded LED method */
	miiphy_write(DEFAULT_MII_NAME, TSEC1_PHY_ADDR, 0x11, reg);
	/* disable LED2, LED1 Link 100/1000, LED0 Activity */
	miiphy_write(DEFAULT_MII_NAME, TSEC1_PHY_ADDR, 0x10, 0x0e4a);
	miiphy_write(DEFAULT_MII_NAME, TSEC1_PHY_ADDR, 0x1F, 0);
	return 0;
}

/*
 * Initialize on-board ethernet devices
 *
 * Returns:
 *      <0, error
 *       0, no ethernet devices found
 *      >0, number of ethernet devices initialized
 */
int board_eth_init(bd_t *bis)
{
	struct fsl_pq_mdio_info mdio_info;
	struct tsec_info_struct tsec_info[2];
	unsigned int num = 0;

#ifdef CONFIG_TSEC1
	SET_STD_TSEC_INFO(tsec_info[num], 1);
	num++;
#endif
#ifdef CONFIG_TSEC2
	SET_STD_TSEC_INFO(tsec_info[num], 2);
	if (is_serdes_configured(SGMII_TSEC2)) {
		puts("eTSEC2 is in SGMII mode.\n");
		tsec_info[num].flags |= TSEC_SGMII;
	}
	num++;
#endif

	mdio_info.regs = (struct tsec_mii_mng *)CONFIG_SYS_MDIO_BASE_ADDR;
	mdio_info.name = DEFAULT_MII_NAME;
	fsl_pq_mdio_init(bis, &mdio_info);

	return tsec_eth_init(bis, tsec_info, num);
}

#ifdef CONFIG_OF_BOARD_SETUP
void ft_board_setup(void *blob, bd_t *bd)
{
	phys_addr_t base;
	phys_size_t size;

	ft_cpu_setup(blob, bd);

	base = getenv_bootm_low();
	size = getenv_bootm_size();
	fdt_fixup_memory(blob, (u64)base, (u64)size);

	FT_FSL_PCI_SETUP;
}
#endif
