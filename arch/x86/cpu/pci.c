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

static struct pci_controller x86_hose;

static void config_pci_bridge(struct pci_controller *hose, pci_dev_t dev,
			      struct pci_config_table *table)
{
	u8 secondary;
	hose->read_byte(hose, dev, PCI_SECONDARY_BUS, &secondary);
	hose->last_busno = max(hose->last_busno, secondary);
	pci_hose_scan_bus(hose, secondary);
}

static struct pci_config_table pci_coreboot_config_table[] = {
	/* vendor, device, class, bus, dev, func */
	{ PCI_ANY_ID, PCI_ANY_ID, PCI_CLASS_BRIDGE_PCI,
		PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, &config_pci_bridge },
	{}
};

void pci_init_board(void)
{
	x86_hose.config_table = pci_coreboot_config_table;
	x86_hose.first_busno = 0;
	x86_hose.last_busno = 0;

	pci_set_region(x86_hose.regions + 0, 0x0, 0x0, 0xffffffff,
		PCI_REGION_MEM);
	x86_hose.region_count = 1;

	pci_setup_type1(&x86_hose);

	pci_register_hose(&x86_hose);

	pci_hose_scan(&x86_hose);
}
