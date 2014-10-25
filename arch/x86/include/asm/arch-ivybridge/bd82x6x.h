/*
 * Copyright (C) 2014 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _ASM_ARCH_BD82X6X_H
#define _ASM_ARCH_BD82X6X_H

struct southbridge_intel_bd82x6x_config {
#if 0
	/**
	 * Interrupt Routing configuration
	 * If bit7 is 1, the interrupt is disabled.
	 */
	uint8_t pirqa_routing;
	uint8_t pirqb_routing;
	uint8_t pirqc_routing;
	uint8_t pirqd_routing;
	uint8_t pirqe_routing;
	uint8_t pirqf_routing;
	uint8_t pirqg_routing;
	uint8_t pirqh_routing;

	/**
	 * GPI Routing configuration
	 *
	 * Only the lower two bits have a meaning:
	 * 00: No effect
	 * 01: SMI# (if corresponding ALT_GPI_SMI_EN bit is also set)
	 * 10: SCI (if corresponding GPIO_EN bit is also set)
	 * 11: reserved
	 */
	uint8_t gpi0_routing;
	uint8_t gpi1_routing;
	uint8_t gpi2_routing;
	uint8_t gpi3_routing;
	uint8_t gpi4_routing;
	uint8_t gpi5_routing;
	uint8_t gpi6_routing;
	uint8_t gpi7_routing;
	uint8_t gpi8_routing;
	uint8_t gpi9_routing;
	uint8_t gpi10_routing;
	uint8_t gpi11_routing;
	uint8_t gpi12_routing;
	uint8_t gpi13_routing;
	uint8_t gpi14_routing;
	uint8_t gpi15_routing;

	uint32_t gpe0_en;
	uint16_t alt_gp_smi_en;
#endif
	/* IDE configuration */
	uint32_t ide_legacy_combined;
	uint32_t sata_ahci;
	uint8_t sata_port_map;
	uint32_t sata_port0_gen3_tx;
	uint32_t sata_port1_gen3_tx;

	/**
	 * SATA Interface Speed Support Configuration
	 *
	 * Only the lower two bits have a meaning:
	 * 00 - No effect (leave as chip default)
	 * 01 - 1.5 Gb/s maximum speed
	 * 10 - 3.0 Gb/s maximum speed
	 * 11 - 6.0 Gb/s maximum speed
	 */
	uint8_t sata_interface_speed_support;
#if 0
	uint32_t gen1_dec;
	uint32_t gen2_dec;
	uint32_t gen3_dec;
	uint32_t gen4_dec;

	/* Enable linear PCIe Root Port function numbers starting at zero */
	uint8_t pcie_port_coalesce;

	/* Override PCIe ASPM */
	uint8_t pcie_aspm_f0;
	uint8_t pcie_aspm_f1;
	uint8_t pcie_aspm_f2;
	uint8_t pcie_aspm_f3;
	uint8_t pcie_aspm_f4;
	uint8_t pcie_aspm_f5;
	uint8_t pcie_aspm_f6;
	uint8_t pcie_aspm_f7;
#endif
};

void bd82x6x_sata_init(pci_dev_t dev, struct southbridge_intel_bd82x6x_config *config);
void bd82x6x_sata_enable(pci_dev_t dev, struct southbridge_intel_bd82x6x_config *config);
void bd82x6x_pci_init(pci_dev_t dev);
void bd82x6x_usb_ehci_init(pci_dev_t dev);
void bd82x6x_usb_xhci_init(pci_dev_t dev);

int bd82x6x_init(void);
void bd82x6x_init_pci_devices(void);

#endif
