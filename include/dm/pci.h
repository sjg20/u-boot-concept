/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2019 Google, Inc
 */

#ifndef __DM_PCI_H
#define __DM_PCI_H

struct udevice;

/**
 * pci_get_devfn() - Extract the devfn from fdt_pci_addr of the device
 *
 * Get devfn from fdt_pci_addr of the specified device
 *
 * @dev:	PCI device
 * @return devfn in bits 15...8 if found, -ENODEV if not found
 */
int pci_get_devfn(struct udevice *dev);

#endif
