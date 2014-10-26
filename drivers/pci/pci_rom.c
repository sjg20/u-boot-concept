/*
 * Copyright (C) 2014 Google, Inc
 *
 * From coreboot, originally based on the Linux kernel (drivers/pci/pci.c).
 *
 * Modifications are:
 * Copyright (C) 2003-2004 Linux Networx
 * (Written by Eric Biederman <ebiederman@lnxi.com> for Linux Networx)
 * Copyright (C) 2003-2006 Ronald G. Minnich <rminnich@gmail.com>
 * Copyright (C) 2004-2005 Li-Ta Lo <ollie@lanl.gov>
 * Copyright (C) 2005-2006 Tyan
 * (Written by Yinghai Lu <yhlu@tyan.com> for Tyan)
 * Copyright (C) 2005-2009 coresystems GmbH
 * (Written by Stefan Reinauer <stepan@coresystems.de> for coresystems GmbH)
 *
 * PCI Bus Services, see include/linux/pci.h for further explanation.
 *
 * Copyright 1993 -- 1997 Drew Eckhardt, Frederic Potter,
 * David Mosberger-Tang
 *
 * Copyright 1997 -- 1999 Martin Mares <mj@atrey.karlin.mff.cuni.cz>

 * SPDX-License-Identifier:	GPL-2.0
 */

#define DEBUG

#include <common.h>
#include <errno.h>
#include <pci.h>
#include <pci_rom.h>

#ifdef CONFIG_HAVE_ACPI_RESUME
#include <asm/acpi.h>
#endif

__weak bool board_should_run_oprom(pci_dev_t dev)
{
	return true;
}

static bool should_load_oprom(pci_dev_t dev)
{
#ifdef CONFIG_HAVE_ACPI_RESUME
	if (acpi_get_slp_type() == 3)
		return false;
#endif
	if (IS_ENABLED(CONFIG_ALWAYS_LOAD_OPROM))
		return 1;
	if (board_should_run_oprom(dev))
		return 1;

	return 0;
}

__weak uint32_t board_map_oprom_vendev(uint32_t vendev)
{
	debug("here\n");
	return vendev;
}

static int pci_rom_probe(pci_dev_t dev, uint class,
			 struct pci_rom_header **hdrp)
{
	struct pci_rom_header *rom_header;
	struct pci_rom_data *rom_data;
	u16 vendor, device;
	u32 vendev;
	u32 mapped_vendev;
	u32 rom_address;

	pci_read_config_word(dev, PCI_VENDOR_ID, &vendor);
	pci_read_config_word(dev, PCI_DEVICE_ID, &device);
	vendev = vendor << 16 | device;
	mapped_vendev = board_map_oprom_vendev(vendev);
	if (vendev != mapped_vendev)
		debug("Device ID mapped to %#08x\n", mapped_vendev);

#ifdef CONFIG_X86_OPTION_ROM_ADDR
	rom_address = CONFIG_X86_OPTION_ROM_ADDR;
#else
	pci_read_config_dword(dev, PCI_ROM_ADDRESS, &rom_address);
	if (rom_address == 0x00000000 || rom_address == 0xffffffff) {
		debug("%s: rom_address=%x\n", __func__, rom_address);
		return -ENOENT;
	}

	/* Enable expansion ROM address decoding. */
	pci_write_config_dword(dev, PCI_ROM_ADDRESS,
			       rom_address | PCI_ROM_ADDRESS_ENABLE);
#endif
	debug("Option ROM address %x\n", rom_address);
	rom_header = (struct pci_rom_header *)rom_address;

	debug("PCI expansion ROM, signature %#04x, "
	       "INIT size %#04x, data ptr %#04x\n",
	       le32_to_cpu(rom_header->signature),
	       rom_header->size * 512, le32_to_cpu(rom_header->data));

	if (le32_to_cpu(rom_header->signature) != PCI_ROM_HDR) {
		printf("Incorrect expansion ROM header signature %04x\n",
		       le32_to_cpu(rom_header->signature));
		return -EINVAL;
	}

	rom_data = (((void *)rom_header) + le32_to_cpu(rom_header->data));

	debug("PCI ROM image, vendor ID %04x, device ID %04x,\n",
	       rom_data->vendor, rom_data->device);

	/* If the device id is mapped, a mismatch is expected */
	if ((vendor != rom_data->vendor
	    || device != rom_data->device)
	    && (vendev == mapped_vendev)) {
		printf("ID mismatch: vendor ID %04x, device ID %04x\n",
		       rom_data->vendor, rom_data->device);
		return -EPERM;
	}

	debug("PCI ROM image, Class Code %04x%02x, "
	       "Code Type %02x\n", rom_data->class_hi, rom_data->class_lo,
	       rom_data->type);

	if (class != ((rom_data->class_hi << 8) | rom_data->class_lo)) {
		debug("Class Code mismatch ROM %08x, dev %08x\n",
		       (rom_data->class_hi << 8) | rom_data->class_lo,
		       class);
	}
	*hdrp = rom_header;

	return 0;
}

int pci_rom_load(pci_dev_t dev, uint16_t class,
		 struct pci_rom_header *rom_header,
		 struct pci_rom_header **ram_headerp)
{
	struct pci_rom_data *rom_data;
	unsigned int rom_size;
	unsigned int image_size = 0;

	do {
		/* Get next image. */
		rom_header = (struct pci_rom_header *)((void *)rom_header
							    + image_size);

		rom_data = (struct pci_rom_data *)((void *)rom_header
				+ le32_to_cpu(rom_header->data));

		image_size = le32_to_cpu(rom_data->ilen) * 512;
	} while ((rom_data->type != 0) && (rom_data->indicator != 0)); // make sure we got x86 version

	if (rom_data->type != 0)
		return -EACCES;

	rom_size = rom_header->size * 512;

	if ((void *)PCI_VGA_RAM_IMAGE_START != rom_header) {
		debug("Copying VGA ROM Image from %p to "
			"0x%x, 0x%x bytes\n", rom_header,
			PCI_VGA_RAM_IMAGE_START, rom_size);
		memcpy((void *)PCI_VGA_RAM_IMAGE_START, rom_header,
			rom_size);
	}
	*ram_headerp = (struct pci_rom_header *)PCI_VGA_RAM_IMAGE_START;

	return 0;
}

int pci_run_vga_bios(pci_dev_t dev)
{
	struct pci_rom_header *rom, *ram;
	uint16_t class;
	int ret;

	/* Only execute VGA ROMs */
	pci_read_config_word(dev, PCI_CLASS_DEVICE, &class);
	if ((class ^ PCI_CLASS_DISPLAY_VGA) & 0xff00) {
		debug("%s: Class %#x, should be %#x\n", __func__, class,
		      PCI_CLASS_DISPLAY_VGA);
		return -ENODEV;
	}

	if (!should_load_oprom(dev))
		return -ENXIO;

	ret = pci_rom_probe(dev, class, &rom);
	if (ret)
		return ret;

	ret = pci_rom_load(dev, class, rom, &ram);
	if (ret)
		return ret;

	if (!board_should_run_oprom(dev))
		return -ENXIO;

// 	run_bios(dev, (unsigned long)ram);

	return 0;
}
