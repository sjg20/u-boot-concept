/*
 * Copyright (C) 2014 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/arch/bd82x6x.h>

pci_dev_t sata_dev = PCI_BDF_CB(0, 0x1f, 2);
struct southbridge_intel_bd82x6x_config sconfig = {
	.ide_legacy_combined = 0,
	.sata_ahci = 1,
	.sata_port_map = 1,
	.sata_port0_gen3_tx = 0x00880a7f,
};

int bd82x6x_init(void)
{
	bd82x6x_pci_init(0);
	bd82x6x_sata_enable(sata_dev, &sconfig);

	return 0;
}

void bd82x6x_init_pci_devices(void)
{
	bd82x6x_sata_init(sata_dev, &sconfig);
	bd82x6x_usb_ehci_init(PCI_BDF_CB(0, 0x1d, 0));
	bd82x6x_usb_ehci_init(PCI_BDF_CB(0, 0x1a, 0));
}
