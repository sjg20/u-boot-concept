/*
 * Copyright (c) 2011 The Chromium OS Authors.
 * (C) Copyright 2008,2009
 * Graeme Russ, <graeme.russ@gmail.com>
 *
 * (C) Copyright 2002
 * Daniel Engström, Omicron Ceti AB, <daniel@omicron.se>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <pci.h>
#include <asm/pci.h>
#include <asm/arch/bd82x6x.h>
#include <asm/arch/pch.h>

static struct pci_controller x86_hose;

int pci_skip_dev(struct pci_controller *hose, pci_dev_t dev)
{
	return dev == PCI_BDF(0, 2, 0);
}

static void config_pci_bridge(struct pci_controller *hose, pci_dev_t dev,
			      struct pci_config_table *table)
{
	u8 secondary;

	pciauto_config_device(hose, dev);
	hose->read_byte(hose, dev, PCI_SECONDARY_BUS, &secondary);
	hose->last_busno = max(hose->last_busno, secondary);
	if (secondary != 0)
		pci_hose_scan_bus(hose, secondary);
}

static struct pci_config_table pci_coreboot_config_table[] = {
	/* vendor, device, class, bus, dev, func */
	{ PCI_ANY_ID, PCI_ANY_ID, PCI_CLASS_BRIDGE_PCI,
		PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, &config_pci_bridge },
	{}
};

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

void pci_init_board(void)
{
	struct pci_controller *hose = &x86_hose;

	hose->config_table = pci_coreboot_config_table;
	hose->first_busno = 0;
	hose->last_busno = 0;

	pci_set_region(hose->regions + 0, 0x0, 0x0, 0xffffffff,
		PCI_REGION_MEM);
	hose->region_count = 1;

	pci_setup_type1(hose);
#ifdef CONFIG_PCI_PNP
	/* PCI memory space */
	pci_set_region(hose->regions + 0,
		       CONFIG_PCI_MEM_BUS,
		       CONFIG_PCI_MEM_PHYS,
		       CONFIG_PCI_MEM_SIZE,
		       PCI_REGION_MEM);

	/* PCI IO space */
	pci_set_region(hose->regions + 1,
		       CONFIG_PCI_IO_BUS,
		       CONFIG_PCI_IO_PHYS,
		       CONFIG_PCI_IO_SIZE,
		       PCI_REGION_IO);
	hose->region_count = 2;
#endif
	pci_register_hose(hose);

	pci_hose_scan(hose);
	hose->last_busno = pci_hose_scan(hose);
	bd82x6x_init_pci_devices();
}
