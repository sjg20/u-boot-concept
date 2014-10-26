/*
 * Copyright (C) 1996-1999 SciTech Software, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef _BIOS_EMUL_H
#define _BIOS_EMUL_H

#ifndef __KERNEL__
/****************************************************************************
REMARKS:
Data structure used to describe the details specific to a particular VGA
controller. This information is used to allow the VGA controller to be
swapped on the fly within the BIOS emulator.

HEADER:
biosemu.h

MEMBERS:
pciInfo         - PCI device information block for the controller
BIOSImage       - Pointer to a read/write copy of the BIOS image
BIOSImageLen    - Length of the BIOS image
LowMem          - Copy of key low memory areas
****************************************************************************/
typedef struct {
	PCIDeviceInfo *pciInfo;
	void *BIOSImage;
	ulong BIOSImageLen;
	uchar LowMem[1536];
} BE_VGAInfo;
#else
/****************************************************************************
REMARKS:
Data structure used to describe the details for the BIOS emulator system
environment as used by the X86 emulator library.

HEADER:
biosemu.h

MEMBERS:
vgaInfo         - VGA BIOS information structure
biosmem_base    - Base of the BIOS image
biosmem_limit   - Limit of the BIOS image
busmem_base     - Base of the VGA bus memory
****************************************************************************/
typedef struct {
	int function;
	int device;
	int bus;
	u32 VendorID;
	u32 DeviceID;
	pci_dev_t pcidev;
	void *BIOSImage;
	u32 BIOSImageLen;
	u8 LowMem[1536];
} BE_VGAInfo;

#endif				/* __KERNEL__ */

#define CRT_C   24		/* 24  CRT Controller Registers             */
#define ATT_C   21		/* 21  Attribute Controller Registers       */
#define GRA_C   9		/* 9   Graphics Controller Registers        */
#define SEQ_C   5		/* 5   Sequencer Registers                  */
#define PAL_C   768		/* 768 Palette Registers                    */

int BootVideoCardBIOS(pci_dev_t pcidev, uchar *bios_rom, int bios_len,
		      BE_VGAInfo **pVGAInfo, int cleanUp);

/* Run a BIOS ROM natively (only supported on x86 machines) */
void bios_run_on_x86(pci_dev_t pcidev, unsigned long addr);

void mainboard_interrupt_handlers(int intXX, int (*intXX_func)(void));

#endif
