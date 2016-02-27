/*
 * Copyright (c) 2016 Google, Inc
 * Parts from coreboot src/soc/intel/broadwell/broadwell_xhci.c
 *
 * SPDX-License-Identifier:	GPL-2.0
 */
#define DEBUG
#include <common.h>
#include <dm.h>
#include <usb.h>
#include <asm/arch/xhci.h>
#include "xhci.h"

static int broadwell_xhci_pci_probe(struct udevice *dev)
{
	struct xhci_hccr *hccr;
	struct xhci_hcor *hcor;
	int len;

	debug("%s: %p, probing %s\n", __func__, dev, dev->name);
	if (!(gd->flags & GD_FLG_RELOC)) {
		ulong xhci_base;

		/* Ensure controller is in D0 state */
		dm_pci_clrset_config16(dev, XHCI_PWR_CTL_STS,
				       XHCI_PWR_CTL_SET_MASK,
				       XHCI_PWR_CTL_SET_D0);

		xhci_base = dm_pci_read_bar32(dev, 0);
		/* Disable Compliance Mode Entry */
		setbits_le32(xhci_base + 0x80ec, 1 << 0);
		return 0;
	}

	printf("base %x\n", dm_pci_read_bar32(dev, 0));

	hccr = (struct xhci_hccr *)dm_pci_map_bar(dev, PCI_BASE_ADDRESS_0,
						  PCI_REGION_MEM);
	len = HC_LENGTH(xhci_readl(&hccr->cr_capbase));
	hcor = (struct xhci_hcor *)((uint32_t)hccr + len);

	debug("Broadwell XHCI-PCI init hccr 0x%x and hcor 0x%x hc_length %d\n",
	      (uint32_t)hccr, (uint32_t)hcor, len);

	return xhci_register(dev, hccr, hcor);
}

static int broadwell_xhci_pci_remove(struct udevice *dev)
{
	int ret;

	ret = xhci_deregister(dev);
	if (ret)
		return ret;

	return 0;
}

static const struct udevice_id broadwell_xhci_pci_ids[] = {
	{ .compatible = "intel,broadwell-xhci" },
	{ }
};

U_BOOT_DRIVER(broadwell_xhci_pci) = {
	.name	= "broadwell_xhci",
	.id	= UCLASS_USB,
	.probe = broadwell_xhci_pci_probe,
	.remove = broadwell_xhci_pci_remove,
	.of_match = broadwell_xhci_pci_ids,
	.ops	= &xhci_usb_ops,
	.platdata_auto_alloc_size = sizeof(struct usb_platdata),
	.priv_auto_alloc_size = sizeof(struct xhci_ctrl),
	.flags	= DM_FLAG_ALLOC_PRIV_DMA,
};
