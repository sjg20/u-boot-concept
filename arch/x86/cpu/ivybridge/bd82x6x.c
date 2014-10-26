/*
 * Copyright (C) 2014 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <errno.h>
#include <malloc.h>
#include <asm/lapic.h>
#include <asm/arch/bd82x6x.h>
#include <asm/arch/model_206ax.h>
#include <asm/arch/sandybridge.h>

pci_dev_t sata_dev = PCI_BDF_CB(0, 0x1f, 2);
struct southbridge_intel_bd82x6x_config sconfig = {
	.ide_legacy_combined = 0,
	.sata_ahci = 1,
	.sata_port_map = 1,
	.sata_port0_gen3_tx = 0x00880a7f,
};

pci_dev_t northbridge_dev = PCI_BDF_CB(0, 0, 0);

int bd82x6x_init(void)
{
	bd82x6x_pci_init(0);
	bd82x6x_sata_enable(sata_dev, &sconfig);
	northbridge_init(northbridge_dev);

	return 0;
}

int bd82x6x_init_pci_devices(void)
{
	struct x86_cpu_priv *cpu;
	pci_dev_t video = PCI_BDF_CB(0, 2, 0);
	int ret;

	bd82x6x_sata_init(sata_dev, &sconfig);
	bd82x6x_usb_ehci_init(PCI_BDF_CB(0, 0x1d, 0));
	bd82x6x_usb_ehci_init(PCI_BDF_CB(0, 0x1a, 0));
	northbridge_enable(northbridge_dev);
	northbridge_init(northbridge_dev);
	northbridge_set_resources(northbridge_dev);

	cpu = calloc(1, sizeof(*cpu));
	if (!cpu)
		return -ENOMEM;
	model_206ax_init(cpu);

	ret = gma_func0_init(video);
	if (ret)
		return ret;

	return 0;
}
