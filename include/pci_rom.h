/*
 * From coreboot file of same name
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _PCI_ROM_H
#define _PCI_ROM_H
 
#define PCI_ROM_HDR			0xaa55
#define PCI_VGA_RAM_IMAGE_START		0xc0000

struct pci_rom_header {
	uint16_t signature;
	uint8_t size;
	uint8_t init[3];
	uint8_t reserved[0x12];
	uint16_t data;
};

struct pci_rom_data {
	uint32_t signature;
	uint16_t vendor;
	uint16_t device;
	uint16_t reserved_1;
	uint16_t dlen;
	uint8_t drevision;
	uint8_t class_lo;
	uint16_t class_hi;
	uint16_t ilen;
	uint16_t irevision;
	uint8_t type;
	uint8_t indicator;
	uint16_t reserved_2;
};

 /**
 * pci_run_vga_bios() - Run the VGA BIOS in an x86 PC
 *
 * @dev:	Video device containing the BIOS
 */
int pci_run_vga_bios(pci_dev_t dev);

uint32_t board_map_oprom_vendev(uint32_t vendev);

#endif
