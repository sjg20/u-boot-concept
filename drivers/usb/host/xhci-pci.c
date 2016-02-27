/*
 * Copyright (c) 2015, Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 * All rights reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <pci.h>
#include <usb.h>

#include "xhci.h"

static int xhci_pci_probe(struct udevice *dev)
{
	struct xhci_hccr *hccr;
	struct xhci_hcor *hcor;
	int len;

	hccr = (struct xhci_hccr *)dm_pci_map_bar(dev, PCI_BASE_ADDRESS_0,
						  PCI_REGION_MEM);
	len = HC_LENGTH(xhci_readl(&hccr->cr_capbase));
	hcor = (struct xhci_hcor *)((uint32_t)hccr + len);

	debug("XHCI-PCI init hccr 0x%x and hcor 0x%x hc_length %d\n",
	      (uint32_t)hccr, (uint32_t)hcor, len);

	return xhci_register(dev, hccr, hcor);
}

static int xhci_pci_remove(struct udevice *dev)
{
	int ret;

	ret = xhci_deregister(dev);
	if (ret)
		return ret;

	return 0;
}

static const struct udevice_id xhci_pci_ids[] = {
	{ .compatible = "xhci-pci" },
	{ }
};

U_BOOT_DRIVER(xhci_pci) = {
	.name	= "xhci_pci",
	.id	= UCLASS_USB,
	.probe = xhci_pci_probe,
	.remove = xhci_pci_remove,
	.of_match = xhci_pci_ids,
	.ops	= &xhci_usb_ops,
	.platdata_auto_alloc_size = sizeof(struct usb_platdata),
	.flags	= DM_FLAG_ALLOC_PRIV_DMA,
};

static struct pci_device_id xhci_pci_supported[] = {
	{ PCI_DEVICE_CLASS(PCI_CLASS_SERIAL_USB_EHCI, ~0) },
	{},
};

U_BOOT_PCI_DEVICE(xhci_pci, xhci_pci_supported);
