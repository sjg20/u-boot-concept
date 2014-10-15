/*
 * From Coreboot
 * Copyright (C) 2008-2009 coresystems GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <asm/arch/pch.h>
#include <asm/arch/bd82x6x.h>

typedef struct southbridge_intel_bd82x6x_config config_t;

static inline u32 sir_read(pci_dev_t dev, int idx)
{
	pci_write_config32(dev, SATA_SIRI, idx);
	return pci_read_config32(dev, SATA_SIRD);
}

static inline void sir_write(pci_dev_t dev, int idx, u32 value)
{
	pci_write_config32(dev, SATA_SIRI, idx);
	pci_write_config32(dev, SATA_SIRD, value);
}

void bd82x6x_sata_init(pci_dev_t dev, struct southbridge_intel_bd82x6x_config *config)
{
	u32 reg32;
	u16 reg16;

	debug("SATA: Initializing...\n");

	/* SATA configuration */

	/* Enable BARs */
	pci_write_config16(dev, PCI_COMMAND, 0x0007);

	if (config->ide_legacy_combined) {
		debug("SATA: Controller in combined mode.\n");

		/* No AHCI: clear AHCI base */
		pci_write_config32(dev, 0x24, 0x00000000);
		/* And without AHCI BAR no memory decoding */
		reg16 = pci_read_config16(dev, PCI_COMMAND);
		reg16 &= ~PCI_COMMAND_MEMORY;
		pci_write_config16(dev, PCI_COMMAND, reg16);

		pci_write_config8(dev, 0x09, 0x80);

		/* Set timings */
		pci_write_config16(dev, IDE_TIM_PRI, IDE_DECODE_ENABLE |
				IDE_ISP_5_CLOCKS | IDE_RCT_4_CLOCKS);
		pci_write_config16(dev, IDE_TIM_SEC, IDE_DECODE_ENABLE |
				IDE_ISP_3_CLOCKS | IDE_RCT_1_CLOCKS |
				IDE_PPE0 | IDE_IE0 | IDE_TIME0);

		/* Sync DMA */
		pci_write_config16(dev, IDE_SDMA_CNT, IDE_SSDE0);
		pci_write_config16(dev, IDE_SDMA_TIM, 0x0200);

		/* Set IDE I/O Configuration */
		reg32 = SIG_MODE_PRI_NORMAL | FAST_PCB1 | FAST_PCB0 | PCB1 | PCB0;
		pci_write_config32(dev, IDE_CONFIG, reg32);

		/* Port enable */
		reg16 = pci_read_config16(dev, 0x92);
		reg16 &= ~0x3f;
		reg16 |= config->sata_port_map;
		pci_write_config16(dev, 0x92, reg16);

		/* SATA Initialization register */
		pci_write_config32(dev, 0x94,
			   ((config->sata_port_map ^ 0x3f) << 24) | 0x183);
	} else if(config->sata_ahci) {
		u32 abar;

		debug("SATA: Controller in AHCI mode.\n");

		/* Set Interrupt Line */
		/* Interrupt Pin is set by D31IP.PIP */
		pci_write_config8(dev, INTR_LN, 0x0a);

		/* Set timings */
		pci_write_config16(dev, IDE_TIM_PRI, IDE_DECODE_ENABLE |
				IDE_ISP_3_CLOCKS | IDE_RCT_1_CLOCKS |
				IDE_PPE0 | IDE_IE0 | IDE_TIME0);
		pci_write_config16(dev, IDE_TIM_SEC, IDE_DECODE_ENABLE |
				IDE_ISP_5_CLOCKS | IDE_RCT_4_CLOCKS);

		/* Sync DMA */
		pci_write_config16(dev, IDE_SDMA_CNT, IDE_PSDE0);
		pci_write_config16(dev, IDE_SDMA_TIM, 0x0001);

		/* Set IDE I/O Configuration */
		reg32 = SIG_MODE_PRI_NORMAL | FAST_PCB1 | FAST_PCB0 | PCB1 | PCB0;
		pci_write_config32(dev, IDE_CONFIG, reg32);

		/* for AHCI, Port Enable is managed in memory mapped space */
		reg16 = pci_read_config16(dev, 0x92);
		reg16 &= ~0x3f; /* 6 ports SKU + ORM */
		reg16 |= 0x8000 | config->sata_port_map;
		pci_write_config16(dev, 0x92, reg16);

		/* SATA Initialization register */
		pci_write_config32(dev, 0x94,
			   ((config->sata_port_map ^ 0x3f) << 24) | 0x183);

		/* Initialize AHCI memory-mapped space */
		abar = pci_read_config32(dev, PCI_BASE_ADDRESS_5) & ~1;
		debug("ABAR: %08X\n", abar);
		/* CAP (HBA Capabilities) : enable power management */
		reg32 = readl(abar + 0x00);
		reg32 |= 0x0c006000;  // set PSC+SSC+SALP+SSS
		reg32 &= ~0x00020060; // clear SXS+EMS+PMS
		/* Set ISS, if available */
		if (config->sata_interface_speed_support)
		{
			reg32 &= ~0x00f00000;
			reg32 |= (config->sata_interface_speed_support & 0x03)
			  << 20;
		}
		writel(reg32, abar + 0x00);
		/* PI (Ports implemented) */
		writel(config->sata_port_map, abar + 0x0c);
		(void) readl(abar + 0x0c); /* Read back 1 */
		(void) readl(abar + 0x0c); /* Read back 2 */
		/* CAP2 (HBA Capabilities Extended)*/
		reg32 = readl(abar + 0x24);
		reg32 &= ~0x00000002;
		writel(reg32, abar + 0x24);
		/* VSP (Vendor Specific Register */
		reg32 = readl(abar + 0xa0);
		reg32 &= ~0x00000005;
		writel(reg32, abar + 0xa0);
	} else {
		debug("SATA: Controller in plain mode.\n");

		/* No AHCI: clear AHCI base */
		pci_write_config32(dev, 0x24, 0x00000000);

		/* And without AHCI BAR no memory decoding */
		reg16 = pci_read_config16(dev, PCI_COMMAND);
		reg16 &= ~PCI_COMMAND_MEMORY;
		pci_write_config16(dev, PCI_COMMAND, reg16);

		/* Native mode capable on both primary and secondary (0xa)
		 * or'ed with enabled (0x50) = 0xf
		 */
		pci_write_config8(dev, 0x09, 0x8f);

		/* Set Interrupt Line */
		/* Interrupt Pin is set by D31IP.PIP */
		pci_write_config8(dev, INTR_LN, 0xff);

		/* Set timings */
		pci_write_config16(dev, IDE_TIM_PRI, IDE_DECODE_ENABLE |
				IDE_ISP_3_CLOCKS | IDE_RCT_1_CLOCKS |
				IDE_PPE0 | IDE_IE0 | IDE_TIME0);
		pci_write_config16(dev, IDE_TIM_SEC, IDE_DECODE_ENABLE |
				IDE_SITRE | IDE_ISP_3_CLOCKS |
				IDE_RCT_1_CLOCKS | IDE_IE0 | IDE_TIME0);

		/* Sync DMA */
		pci_write_config16(dev, IDE_SDMA_CNT, IDE_SSDE0 | IDE_PSDE0);
		pci_write_config16(dev, IDE_SDMA_TIM, 0x0201);

		/* Set IDE I/O Configuration */
		reg32 = SIG_MODE_PRI_NORMAL | FAST_PCB1 | FAST_PCB0 | PCB1 | PCB0;
		pci_write_config32(dev, IDE_CONFIG, reg32);

		/* Port enable */
		reg16 = pci_read_config16(dev, 0x92);
		reg16 &= ~0x3f;
		reg16 |= config->sata_port_map;
		pci_write_config16(dev, 0x92, reg16);

		/* SATA Initialization register */
		pci_write_config32(dev, 0x94,
			   ((config->sata_port_map ^ 0x3f) << 24) | 0x183);
	}

	/* Set Gen3 Transmitter settings if needed */
	if (config->sata_port0_gen3_tx)
		pch_iobp_update(SATA_IOBP_SP0G3IR, 0,
				config->sata_port0_gen3_tx);

	if (config->sata_port1_gen3_tx)
		pch_iobp_update(SATA_IOBP_SP1G3IR, 0,
				config->sata_port1_gen3_tx);

	/* Additional Programming Requirements */
	sir_write(dev, 0x04, 0x00001600);
	sir_write(dev, 0x28, 0xa0000033);
	reg32 = sir_read(dev, 0x54);
	reg32 &= 0xff000000;
	reg32 |= 0x5555aa;
	sir_write(dev, 0x54, reg32);
	sir_write(dev, 0x64, 0xcccc8484);
	reg32 = sir_read(dev, 0x68);
	reg32 &= 0xffff0000;
	reg32 |= 0xcccc;
	sir_write(dev, 0x68, reg32);
	reg32 = sir_read(dev, 0x78);
	reg32 &= 0x0000ffff;
	reg32 |= 0x88880000;
	sir_write(dev, 0x78, reg32);
	sir_write(dev, 0x84, 0x001c7000);
	sir_write(dev, 0x88, 0x88338822);
	sir_write(dev, 0xa0, 0x001c7000);
	// a4
	sir_write(dev, 0xc4, 0x0c0c0c0c);
	sir_write(dev, 0xc8, 0x0c0c0c0c);
	sir_write(dev, 0xd4, 0x10000000);

	pch_iobp_update(0xea004001, 0x3fffffff, 0xc0000000);
	pch_iobp_update(0xea00408a, 0xfffffcff, 0x00000100);
}

void bd82x6x_sata_enable(pci_dev_t dev, struct southbridge_intel_bd82x6x_config *config)
{
	u16 map = 0;

	/*
	 * Set SATA controller mode early so the resource allocator can
	 * properly assign IO/Memory resources for the controller.
	 */
	if (config->sata_ahci)
		map = 0x0060;

	map |= (config->sata_port_map ^ 0x3f) << 8;

	pci_write_config16(dev, 0x90, map);
}

#if 0
static void sata_set_subsystem(device_t dev, unsigned vendor, unsigned device)
{
	if (!vendor || !device) {
		pci_write_config32(dev, PCI_SUBSYSTEM_VENDOR_ID,
				pci_read_config32(dev, PCI_VENDOR_ID));
	} else {
		pci_write_config32(dev, PCI_SUBSYSTEM_VENDOR_ID,
				((device & 0xffff) << 16) | (vendor & 0xffff));
	}
}

static struct pci_operations sata_pci_ops = {
	.set_subsystem    = sata_set_subsystem,
};

static struct device_operations sata_ops = {
	.read_resources		= pci_dev_read_resources,
	.set_resources		= pci_dev_set_resources,
	.enable_resources	= pci_dev_enable_resources,
	.init			= sata_init,
	.enable			= sata_enable,
	.scan_bus		= 0,
	.ops_pci		= &sata_pci_ops,
};

static const unsigned short pci_device_ids[] = { 0x1c00, 0x1c01, 0x1c02, 0x1c03,
						 0x1e00, 0x1e01, 0x1e02, 0x1e03,
						 0 };

static const struct pci_driver pch_sata __pci_driver = {
	.ops	 = &sata_ops,
	.vendor	 = PCI_VENDOR_ID_INTEL,
	.devices = pci_device_ids,
};
#endif
