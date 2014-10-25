/*
 * From Coreboot
 * Copyright (C) 2008-2009 coresystems GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <asm/arch/pch.h>

void bd82x6x_usb_ehci_init(pci_dev_t dev)
{
	u32 reg32;

	/* Disable Wake on Disconnect in RMH */
	reg32 = RCBA32(0x35b0);
	reg32 |= 0x22;
	RCBA32(0x35b0) = reg32;

	debug("EHCI: Setting up controller.. ");
	reg32 = pci_read_config32(dev, PCI_COMMAND);
	reg32 |= PCI_COMMAND_MASTER;
	//reg32 |= PCI_COMMAND_SERR;
	pci_write_config32(dev, PCI_COMMAND, reg32);

	debug("done.\n");
}

#if 0
static void usb_ehci_set_subsystem(pci_dev_t dev, unsigned vendor, unsigned device)
{
	u8 access_cntl;

	access_cntl = pci_read_config8(dev, 0x80);

	/* Enable writes to protected registers. */
	pci_write_config8(dev, 0x80, access_cntl | 1);

	if (!vendor || !device) {
		pci_write_config32(dev, PCI_SUBSYSTEM_VENDOR_ID,
				pci_read_config32(dev, PCI_VENDOR_ID));
	} else {
		pci_write_config32(dev, PCI_SUBSYSTEM_VENDOR_ID,
				((device & 0xffff) << 16) | (vendor & 0xffff));
	}

	/* Restore protection. */
	pci_write_config8(dev, 0x80, access_cntl);
}

static void usb_ehci_set_resources(pci_dev_t dev)
{
#if CONFIG_USBDEBUG
	struct resource *res;
	u32 base;
	u32 usb_debug;

	usb_debug = get_ehci_debug();
	set_ehci_debug(0);
#endif
	pci_dev_set_resources(dev);

#if CONFIG_USBDEBUG
	res = find_resource(dev, 0x10);
	set_ehci_debug(usb_debug);
	if (!res) return;
	base = res->base;
	set_ehci_base(base);
	report_resource_stored(dev, res, "");
#endif
}



static struct pci_operations lops_pci = {
	.set_subsystem	= &usb_ehci_set_subsystem,
};

static struct device_operations usb_ehci_ops = {
	.read_resources		= pci_dev_read_resources,
	.set_resources		= usb_ehci_set_resources,
	.enable_resources	= pci_dev_enable_resources,
	.init			= usb_ehci_init,
	.scan_bus		= 0,
	.ops_pci		= &lops_pci,
};

static const unsigned short pci_device_ids[] = { 0x1c26, 0x1c2d, 0x1e26, 0x1e2d,
						 0 };

static const struct pci_driver pch_usb_ehci __pci_driver = {
	.ops	 = &usb_ehci_ops,
	.vendor	 = PCI_VENDOR_ID_INTEL,
	.devices = pci_device_ids,
};
#endif
