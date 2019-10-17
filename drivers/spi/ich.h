/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2011 The Chromium OS Authors.
 *
 * This file is derived from the flashrom project.
 */

#ifndef _ICH_H_
#define _ICH_H_

struct ich7_spi_regs {
	uint16_t spis;
	uint16_t spic;
	uint32_t spia;
	uint64_t spid[8];
	uint64_t _pad;
	uint32_t bbar;
	uint16_t preop;
	uint16_t optype;
	uint8_t opmenu[8];
} __packed;

struct ich9_spi_regs {
	uint32_t bfpr;		/* 0x00 */
	uint16_t hsfs;
	uint16_t hsfc;
	uint32_t faddr;
	uint32_t _reserved0;
	uint32_t fdata[16];	/* 0x10 */
	uint32_t frap;		/* 0x50 */
	uint32_t freg[5];
	uint32_t _reserved1[3];
	uint32_t pr[5];		/* 0x74 */
	uint32_t _reserved2[2];
	uint8_t ssfs;		/* 0x90 */
	uint8_t ssfc[3];
	uint16_t preop;		/* 0x94 */
	uint16_t optype;
	uint8_t opmenu[8];	/* 0x98 */
	uint32_t bbar;
	uint8_t _reserved3[12];
	uint32_t fdoc;		/* 0xb0 */
	uint32_t fdod;
	uint8_t _reserved4[8];
	uint32_t afc;		/* 0xc0 */
	uint32_t lvscc;
	uint32_t uvscc;
	uint8_t _reserved5[4];
	uint32_t fpb;		/* 0xd0 */
	uint8_t _reserved6[28];
	uint32_t srdl;		/* 0xf0 */
	uint32_t srdc;
	uint32_t scs;
	uint32_t bcr;
} __packed;

/* Register offsets from the MMIO region base (PCI_BASE_ADDRESS_0) */
struct fast_spi_regs {
	u32 bfp;
	u32 hsfsts_ctl;
	u32 faddr;
	u32 dlock;

	u32 fdata[0x10];

	u32 fracc;
	u32 freg[12];
	u32 fpr[5];
	u32 gpr0;
	u32 spare2;
	u32 sts_ctl;
	u16 preop;		/* a4 */
	u16 optype;
	u8 opmenu[8];

	u32 spare3;
	u32 fdoc;
	u32 fdod;
	u32 spare4;
	u32 afc;
	u32 vscc[2];
	u32 ptinx;
	u32 ptdata;
};
check_member(fast_spi_regs, ptdata, 0xd0);

enum {
	SPIS_SCIP =		0x0001,
	SPIS_GRANT =		0x0002,
	SPIS_CDS =		0x0004,
	SPIS_FCERR =		0x0008,
	SSFS_AEL =		0x0010,
	SPIS_LOCK =		0x8000,
	SPIS_RESERVED_MASK =	0x7ff0,
	SSFS_RESERVED_MASK =	0x7fe2
};

enum {
	SPIC_SCGO =		0x000002,
	SPIC_ACS =		0x000004,
	SPIC_SPOP =		0x000008,
	SPIC_DBC =		0x003f00,
	SPIC_DS =		0x004000,
	SPIC_SME =		0x008000,
	SSFC_SCF_MASK =		0x070000,
	SSFC_RESERVED =		0xf80000,

	/* Mask for speed byte, biuts 23:16 of SSFC */
	SSFC_SCF_33MHZ	=	0x01,

	/* Shift the above values left this much for APL */
	SPIC_APL_SHIFT =	8,
};

enum {
	HSFS_FDONE =		0x0001,
	HSFS_FCERR =		0x0002,
	HSFS_AEL =		0x0004,
	HSFS_BERASE_MASK =	0x0018,
	HSFS_BERASE_SHIFT =	3,
	HSFS_SCIP =		0x0020,
	HSFS_FDOPSS =		0x2000,
	HSFS_FDV =		0x4000,
	HSFS_FLOCKDN =		0x8000
};

enum {
	HSFC_FGO =		0x0001,
	HSFC_FCYCLE_MASK =	0x0006,
	HSFC_FCYCLE_SHIFT =	1,
	HSFC_FDBC_MASK =	0x3f00,
	HSFC_FDBC_SHIFT =	8,
	HSFC_FSMIE =		0x8000
};

struct spi_trans {
	uint8_t cmd;
	const uint8_t *out;
	uint32_t bytesout;
	uint8_t *in;
	uint32_t bytesin;
	uint8_t type;
	uint8_t opcode;
	uint32_t offset;
};

#define SPI_OPCODE_WRSR		0x01
#define SPI_OPCODE_PAGE_PROGRAM	0x02
#define SPI_OPCODE_READ		0x03
#define SPI_OPCODE_WRDIS	0x04
#define SPI_OPCODE_RDSR		0x05
#define SPI_OPCODE_WREN		0x06
#define SPI_OPCODE_FAST_READ	0x0b
#define SPI_OPCODE_ERASE_SECT	0x20
#define SPI_OPCODE_READ_ID	0x9f
#define SPI_OPCODE_ERASE_BLOCK	0xd8

#define SPI_OPCODE_TYPE_READ_NO_ADDRESS		0
#define SPI_OPCODE_TYPE_WRITE_NO_ADDRESS	1
#define SPI_OPCODE_TYPE_READ_WITH_ADDRESS	2
#define SPI_OPCODE_TYPE_WRITE_WITH_ADDRESS	3

#define SPI_OPMENU_0	SPI_OPCODE_WRSR
#define SPI_OPTYPE_0	SPI_OPCODE_TYPE_WRITE_NO_ADDRESS

#define SPI_OPMENU_1	SPI_OPCODE_PAGE_PROGRAM
#define SPI_OPTYPE_1	SPI_OPCODE_TYPE_WRITE_WITH_ADDRESS

#define SPI_OPMENU_2	SPI_OPCODE_READ
#define SPI_OPTYPE_2	SPI_OPCODE_TYPE_READ_WITH_ADDRESS

#define SPI_OPMENU_3	SPI_OPCODE_RDSR
#define SPI_OPTYPE_3	SPI_OPCODE_TYPE_READ_NO_ADDRESS

#define SPI_OPMENU_4	SPI_OPCODE_ERASE_SECT
#define SPI_OPTYPE_4	SPI_OPCODE_TYPE_WRITE_WITH_ADDRESS

#define SPI_OPMENU_5	SPI_OPCODE_READ_ID
#define SPI_OPTYPE_5	SPI_OPCODE_TYPE_READ_NO_ADDRESS

#define SPI_OPMENU_6	SPI_OPCODE_ERASE_BLOCK
#define SPI_OPTYPE_6	SPI_OPCODE_TYPE_WRITE_WITH_ADDRESS

#define SPI_OPMENU_7	SPI_OPCODE_FAST_READ
#define SPI_OPTYPE_7	SPI_OPCODE_TYPE_READ_WITH_ADDRESS

#define SPI_OPPREFIX	((SPI_OPCODE_WREN << 8) | SPI_OPCODE_WREN)
#define SPI_OPTYPE	((SPI_OPTYPE_7 << 14) | (SPI_OPTYPE_6 << 12) | \
			 (SPI_OPTYPE_5 << 10) | (SPI_OPTYPE_4 <<  8) | \
			 (SPI_OPTYPE_3 <<  6) | (SPI_OPTYPE_2 <<  4) | \
			 (SPI_OPTYPE_1 <<  2) | (SPI_OPTYPE_0 <<  0))
#define SPI_OPMENU_UPPER ((SPI_OPMENU_7 << 24) | (SPI_OPMENU_6 << 16) | \
			  (SPI_OPMENU_5 <<  8) | (SPI_OPMENU_4 <<  0))
#define SPI_OPMENU_LOWER ((SPI_OPMENU_3 << 24) | (SPI_OPMENU_2 << 16) | \
			  (SPI_OPMENU_1 <<  8) | (SPI_OPMENU_0 <<  0))

#define ICH_BOUNDARY	0x1000

/* PCI configuration registers */
#define SPIBAR_BIOS_CONTROL			0xdc
#define SPIBAR_BIOS_CONTROL_WPD			BIT(0)
#define SPIBAR_BIOS_CONTROL_LOCK_ENABLE		BIT(1)
#define SPIBAR_BIOS_CONTROL_CACHE_DISABLE	BIT(2)
#define SPIBAR_BIOS_CONTROL_PREFETCH_ENABLE	BIT(3)
#define SPIBAR_BIOS_CONTROL_EISS		BIT(5)
#define SPIBAR_BIOS_CONTROL_BILD		BIT(7)

#define SPIBAR_HSFSTS_FDBC_MASK	(0x3f << 24)
#define SPIBAR_HSFSTS_FDBC(n)		(((n) << 24) & SPIBAR_HSFSTS_FDBC_MASK)
#define SPIBAR_HSFSTS_WET		BIT(21)
#define SPIBAR_HSFSTS_FCYCLE_MASK	(0xf << 17)
#define SPIBAR_HSFSTS_FCYCLE(cyc)	(((cyc) << 17) \
					& SPIBAR_HSFSTS_FCYCLE_MASK)

/* Supported flash cycle types */
#define SPIBAR_HSFSTS_CYCLE_READ	SPIBAR_HSFSTS_FCYCLE(0)
#define SPIBAR_HSFSTS_CYCLE_WRITE	SPIBAR_HSFSTS_FCYCLE(2)
#define SPIBAR_HSFSTS_CYCLE_4K_ERASE	SPIBAR_HSFSTS_FCYCLE(3)
#define SPIBAR_HSFSTS_CYCLE_64K_ERASE	SPIBAR_HSFSTS_FCYCLE(4)
#define SPIBAR_HSFSTS_CYCLE_RDID	SPIBAR_HSFSTS_FCYCLE(6)
#define SPIBAR_HSFSTS_CYCLE_WR_STATUS	SPIBAR_HSFSTS_FCYCLE(7)
#define SPIBAR_HSFSTS_CYCLE_RD_STATUS	SPIBAR_HSFSTS_FCYCLE(8)

#define SPIBAR_HSFSTS_FGO		BIT(16)
#define SPIBAR_HSFSTS_FLOCKDN		BIT(15)
#define SPIBAR_HSFSTS_FDV		BIT(14)
#define SPIBAR_HSFSTS_FDOPSS		BIT(13)
#define SPIBAR_HSFSTS_WRSDIS		BIT(11)
#define SPIBAR_HSFSTS_SAF_CE		BIT(8)
#define SPIBAR_HSFSTS_SAF_ACTIVE	BIT(7)
#define SPIBAR_HSFSTS_SAF_LE		BIT(6)
#define SPIBAR_HSFSTS_SCIP		BIT(5)
#define SPIBAR_HSFSTS_SAF_DLE		BIT(4)
#define SPIBAR_HSFSTS_SAF_ERROR		BIT(3)
#define SPIBAR_HSFSTS_AEL		BIT(2)
#define SPIBAR_HSFSTS_FCERR		BIT(1)
#define SPIBAR_HSFSTS_FDONE		BIT(0)
#define SPIBAR_HSFSTS_W1C_BITS		0xff

/* Maximum bytes of data that can fit in FDATAn (0x10) registers */
#define SPIBAR_FDATA_FIFO_SIZE		0x40

#define SPIBAR_HWSEQ_XFER_TIMEOUT_MS	500

enum ich_version {
	ICHV_7,
	ICHV_9,
	ICHV_APL,
};

struct ich_spi_priv {
	int opmenu;
	int menubytes;
	void *base;		/* Base of register set */
	int preop;
	int optype;
	int addr;
	int data;
	unsigned databytes;
	int status;
	int control;
	int bbar;
	int bcr;
	uint32_t *pr;		/* only for ich9 */
	int speed;		/* pointer to speed control */
	ulong max_speed;	/* Maximum bus speed in MHz */
	ulong cur_speed;	/* Current bus speed */
	struct spi_trans trans;	/* current transaction in progress */
};

#endif /* _ICH_H_ */
