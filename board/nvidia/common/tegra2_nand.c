/*
 * (C) Copyright 2006 Detlev Zundel, dzu@denx.de
 * (C) Copyright 2006 DENX Software Engineering
 * (C) Copyright 2011 NVIDIA Corporation <www.nvidia.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/io.h>
#include <nand.h>
#include <asm/arch/clk_rst.h>
#include <asm/arch/clock.h>
#include <asm/clocks.h>
#include <asm/arch/pinmux.h>
#include <asm/arch/gpio.h>
#include <asm/errno.h>
#include "tegra2_nand.h"

#define NAND_CMD_TIMEOUT_MS		10

static struct nand_ecclayout eccoob = {
	.eccbytes = CONFIG_NAND_RS_DATA_ECC_BYTES +
		CONFIG_NAND_TAG_ECC_BYTES,
	.eccpos = {
		4,  5,  6,  7,  8,  9,  10, 11, 12,
		13, 14, 15, 16, 17, 18, 19, 20, 21,
		22, 23, 24, 25, 26, 27, 28, 29, 30,
		31, 32, 33, 34, 35, 36, 37, 38, 39,
		60, 61, 62, 63,
	},
	.oobavail = CONFIG_NAND_TAG_BYTES,
	.oobfree = {
		{ .offset = (CONFIG_NAND_SKIPPED_SPARE_BYTES +
				CONFIG_NAND_RS_DATA_ECC_BYTES),
		  .length = CONFIG_NAND_TAG_BYTES,
		},
	},
};

enum {
	ECC_OK,
	ECC_TAG_ERROR = 1 << 0,
	ECC_DATA_ERROR = 1 << 1
};

struct nand_info {
	struct nand_ctlr *reg;
/*
 * When running in PIO mode to get READ ID bytes from register
 * RESP_0, we need this variable as an index to know which byte in
 * register RESP_0 should be read.
 * Because common code in nand_base.c invokes read_byte function two times
 * for NAND_CMD_READID.
 * And our controller returns 4 bytes at once in register RESP_0.
 */
	int	pio_byte_index;
	int	bus_width;	/* n bits */
	int	page_data_bytes; /* n bytes in the data area of each page */
	int	page_spare_bytes;/* n bytes in the spare area of each page */
	int	skipped_spare_bytes;	/* At the beginning of spare area. */
	int	rs_data_ecc_bytes;	/* How many ECC bytes for data area */
	int	tag_bytes;	/* How many tag bytes in spare area */
	int	tag_ecc_bytes;	/* How many ECC bytes for tag bytes */
	int	wp_gpio;	/* GPIO to enable WRITE */
};

struct nand_info nand_ctrl;

/**
 * nand_waitfor_cmd_completion - wait for command completion
 * @param reg:	nand_ctlr structure
 * @return:
 *	1 - Command completed
 *	0 - Timeout
 */
static int nand_waitfor_cmd_completion(struct nand_ctlr *reg)
{
	int i;
	u32 reg_val;

	for (i = 0; i < NAND_CMD_TIMEOUT_MS * 1000; i++) {
		if ((readl(&reg->command) & CMD_GO) ||
			!(readl(&reg->status) &
				STATUS_RBSY0) ||
			!(readl(&reg->isr) &
				ISR_IS_CMD_DONE)) {
			udelay(1);
			continue;
		}
		reg_val = readl(&reg->dma_mst_ctrl);
		/*
		 * If DMA_MST_CTRL_EN_A_ENABLE or
		 * DMA_MST_CTRL_EN_B_ENABLE is set,
		 * that means DMA engine is running, then we
		 * have to wait until
		 * DMA_MST_CTRL_IS_DMA_DONE
		 * is cleared for DMA transfer completion.
		 */
		if (reg_val & (DMA_MST_CTRL_EN_A_ENABLE |
			DMA_MST_CTRL_EN_B_ENABLE)) {
			if (reg_val & DMA_MST_CTRL_IS_DMA_DONE)
				return 1;
		} else
			return 1;
		udelay(1);
	}
	return 0;
}

/**
 * nand_read_byte - [DEFAULT] read one byte from the chip
 * @param mtd:	MTD device structure
 * @return:	data byte
 *
 * Default read function for 8bit bus-width
 */
static uint8_t nand_read_byte(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	int dword_read;
	struct nand_info *nand_config;

	nand_config = (struct nand_info *) chip->priv;

	dword_read = readl(&nand_config->reg->resp);
	dword_read = dword_read >> (8 * nand_config->pio_byte_index);
	nand_config->pio_byte_index++;
	return (uint8_t) dword_read;
}

/**
 * nand_write_buf - [DEFAULT] write buffer to chip
 * @param mtd:	MTD device structure
 * @param buf:	data buffer
 * @param len:	number of bytes to write
 *
 * Default write function for 8bit bus-width
 */
static void nand_write_buf(struct mtd_info *mtd, const uint8_t *buf,
	int len)
{
	int i, j, l;
	struct nand_chip *chip = mtd->priv;
	struct nand_info *nand_config;

	nand_config = (struct nand_info *) chip->priv;

	for (i = 0; i < len / 4; i++) {
		l = ((int *)buf)[i];
		writel(l, &nand_config->reg->resp);
		writel(CMD_GO | CMD_PIO | CMD_TX |
			(CMD_TRANS_SIZE_BYTES4 <<
				CMD_TRANS_SIZE_SHIFT)
			| CMD_A_VALID | CMD_CE0,
			&nand_config->reg->command);

		if (!nand_waitfor_cmd_completion(nand_config->reg))
			printf("Command timeout during write_buf\n");
	}
	if (len & 3) {
		l = 0;
		for (j = 0; j < (len & 3); j++)
			l |= (((int) buf[i * 4 + j]) << (8 * j));

		writel(l, &nand_config->reg->resp);
		writel(CMD_GO | CMD_PIO | CMD_TX |
			(((len & 3) - 1) << CMD_TRANS_SIZE_SHIFT) |
			CMD_A_VALID | CMD_CE0,
			&nand_config->reg->command);
		if (!nand_waitfor_cmd_completion(nand_config->reg))
			printf("Command timeout during write_buf\n");
	}
}

/**
 * nand_read_buf - [DEFAULT] read chip data into buffer
 * @param mtd:	MTD device structure
 * @param buf:	buffer to store date
 * @param len:	number of bytes to read
 *
 * Default read function for 8bit bus-width
 */
static void nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	int i, j, l;
	struct nand_chip *chip = mtd->priv;
	int *buf_dword;
	struct nand_info *nand_config;

	nand_config = (struct nand_info *) chip->priv;

	buf_dword = (int *) buf;
	for (i = 0; i < len / 4; i++) {
		writel(CMD_GO | CMD_PIO | CMD_RX |
			(CMD_TRANS_SIZE_BYTES4 <<
				CMD_TRANS_SIZE_SHIFT)
			| CMD_A_VALID | CMD_CE0,
			&nand_config->reg->command);
		if (!nand_waitfor_cmd_completion(nand_config->reg))
			printf("Command timeout during read_buf\n");
		l = readl(&nand_config->reg->resp);
		buf_dword[i] = l;
	}
	if (len & 3) {
		writel(CMD_GO | CMD_PIO | CMD_RX |
			(((len & 3) - 1) << CMD_TRANS_SIZE_SHIFT) |
			CMD_A_VALID | CMD_CE0,
			&nand_config->reg->command);
		if (!nand_waitfor_cmd_completion(nand_config->reg))
			printf("Command timeout during read_buf\n");
		l = readl(&nand_config->reg->resp);
		for (j = 0; j < (len & 3); j++)
			buf[i * 4 + j] = (char) (l >> (8 * j));
	}
}

/**
 * nand_dev_ready - check NAND status is ready or not
 * @param mtd:	MTD device structure
 * @return:
 *	1 - ready
 *	0 - not ready
 */
static int nand_dev_ready(struct mtd_info *mtd)
{
	register struct nand_chip *chip = mtd->priv;
	int reg_val;
	struct nand_info *nand_config;

	nand_config = (struct nand_info *) chip->priv;

	reg_val = readl(&nand_config->reg->status);
	if (reg_val & STATUS_RBSY0)
		return 1;
	else
		return 0;
}

/* Hardware specific access to control-lines */
static void nand_hwcontrol(struct mtd_info *mtd, int cmd,
	unsigned int ctrl)
{
}

/**
 * nand_clear_interrupt_status - clear all interrupt status bits
 * @param reg:	nand_ctlr structure
 */
static void nand_clear_interrupt_status(struct nand_ctlr *reg)
{
	u32 reg_val;

	/* Clear interrupt status */
	reg_val = readl(&reg->isr);
	writel(reg_val, &reg->isr);
}

/**
 * nand_command - [DEFAULT] Send command to NAND device
 * @param mtd:		MTD device structure
 * @param command:	the command to be sent
 * @param column:	the column address for this command, -1 if none
 * @param page_addr:	the page address for this command, -1 if none
 */
static void nand_command(struct mtd_info *mtd, unsigned int command,
	int column, int page_addr)
{
	register struct nand_chip *chip = mtd->priv;
	struct nand_info *nand_config;

	nand_config = (struct nand_info *) chip->priv;

	/*
	 * Write out the command to the device.
	 */
	if (mtd->writesize < 2048) {
		/*
		 * Only command NAND_CMD_RESET or NAND_CMD_READID will come
		 * here before mtd->writesize is initialized, we don't have
		 * any action here because page size of NAND HY27UF084G2B
		 * is 2048 bytes and mtd->writesize will be 2048 after
		 * initialized.
		 */
	} else {
		/* Emulate NAND_CMD_READOOB */
		if (command == NAND_CMD_READOOB) {
			column += mtd->writesize;
			command = NAND_CMD_READ0;
		}

		/* Adjust columns for 16 bit bus-width */
		if (column != -1 && (chip->options & NAND_BUSWIDTH_16))
			column >>= 1;
	}

	nand_clear_interrupt_status(nand_config->reg);

	/* Stop DMA engine, clear DMA completion status */
	writel(DMA_MST_CTRL_EN_A_DISABLE
		| DMA_MST_CTRL_EN_B_DISABLE
		| DMA_MST_CTRL_IS_DMA_DONE,
		&nand_config->reg->dma_mst_ctrl);

	/*
	 * Program and erase have their own busy handlers
	 * status and sequential in needs no delay
	 */
	switch (command) {
	case NAND_CMD_READID:
		writel(NAND_CMD_READID, &nand_config->reg->cmd_reg1);
		writel(CMD_GO | CMD_CLE | CMD_ALE | CMD_PIO
			| CMD_RX |
			(CMD_TRANS_SIZE_BYTES4 << CMD_TRANS_SIZE_SHIFT)
			| CMD_CE0,
			&nand_config->reg->command);
		nand_config->pio_byte_index = 0;
		break;
	case NAND_CMD_READ0:
		writel(NAND_CMD_READ0, &nand_config->reg->cmd_reg1);
		writel(NAND_CMD_READSTART, &nand_config->reg->cmd_reg2);
		writel((page_addr << 16) | (column & 0xFFFF),
			&nand_config->reg->addr_reg1);
		writel(page_addr >> 16, &nand_config->reg->addr_reg2);
		return;
	case NAND_CMD_SEQIN:
		writel(NAND_CMD_SEQIN, &nand_config->reg->cmd_reg1);
		writel(NAND_CMD_PAGEPROG, &nand_config->reg->cmd_reg2);
		writel((page_addr << 16) | (column & 0xFFFF),
			&nand_config->reg->addr_reg1);
		writel(page_addr >> 16,
			&nand_config->reg->addr_reg2);
		return;
	case NAND_CMD_PAGEPROG:
		return;
	case NAND_CMD_ERASE1:
		writel(NAND_CMD_ERASE1, &nand_config->reg->cmd_reg1);
		writel(NAND_CMD_ERASE2, &nand_config->reg->cmd_reg2);
		writel(page_addr, &nand_config->reg->addr_reg1);
		writel(CMD_GO | CMD_CLE | CMD_ALE |
			CMD_SEC_CMD | CMD_CE0 | CMD_ALE_BYTES3,
			&nand_config->reg->command);
		break;
	case NAND_CMD_RNDOUT:
		return;
	case NAND_CMD_ERASE2:
		return;
	case NAND_CMD_STATUS:
		writel(NAND_CMD_STATUS, &nand_config->reg->cmd_reg1);
		writel(CMD_GO | CMD_CLE | CMD_PIO | CMD_RX
			| (CMD_TRANS_SIZE_BYTES1 <<
				CMD_TRANS_SIZE_SHIFT)
			| CMD_CE0,
			&nand_config->reg->command);
		nand_config->pio_byte_index = 0;
		break;
	case NAND_CMD_RESET:
		writel(NAND_CMD_RESET, &nand_config->reg->cmd_reg1);
		writel(CMD_GO | CMD_CLE | CMD_CE0,
			&nand_config->reg->command);
		break;
	default:
		return;
	}
	if (!nand_waitfor_cmd_completion(nand_config->reg))
		printf("Command 0x%02X timeout\n", command);
}

/*
 * blank_check - check whether the pointed buffer are all FF (blank).
 * @param: buf - data buffer for blank check
 * @param: len - length of the buffer in byte
 * @return:
 *	1 - blank
 *	0 - non-blank
 */
static int blank_check(u8 *buf, int len)
{
	int i;

	for (i = 0; i < len; i++)
		if (buf[i] != 0xFF)
			return 0;
	return 1;
}

/*
 * check_ecc_error - After a DMA transfer for read, we call this function to
 * see whether there is any uncorrectable error on the pointed data buffer
 * or oob buffer.
 *
 * @param reg:		nand_ctlr structure
 * @param databuf:	data buffer
 * @param a_len:	data buffer length
 * @param oobbuf:	oob buffer
 * @param b_len:	oob buffer length
 * @return:
 *	ECC_OK - no ECC error or correctable ECC error
 *	ECC_TAG_ERROR - uncorrectable tag ECC error
 *	ECC_DATA_ERROR - uncorrectable data ECC error
 *	ECC_DATA_ERROR + ECC_TAG_ERROR - uncorrectable data+tag ECC error
 */
static int check_ecc_error(struct nand_ctlr *reg, u8 *databuf,
	int a_len, u8 *oobbuf, int b_len)
{
	int return_val = ECC_OK;
	u32 reg_val;

	if (!(readl(&reg->isr) & ISR_IS_ECC_ERR))
		return ECC_OK;

	reg_val = readl(&reg->dec_status);
	if ((reg_val & DEC_STATUS_A_ECC_FAIL) && databuf) {
		reg_val = readl(&reg->bch_dec_status_buf);
		/*
		 * If uncorrectable error occurs on data area, then see whether
		 * they are all FF. If all are FF, it's a blank page.
		 * Not error.
		 */
		if ((reg_val & BCH_DEC_STATUS_FAIL_SEC_FLAG_MASK) &&
			!blank_check(databuf, a_len))
			return_val |= ECC_DATA_ERROR;
	}

	if ((reg_val & DEC_STATUS_B_ECC_FAIL) && oobbuf) {
		reg_val = readl(&reg->bch_dec_status_buf);
		/*
		 * If uncorrectable error occurs on tag area, then see whether
		 * they are all FF. If all are FF, it's a blank page.
		 * Not error.
		 */
		if ((reg_val & BCH_DEC_STATUS_FAIL_TAG_MASK) &&
			!blank_check(oobbuf, b_len))
			return_val |= ECC_TAG_ERROR;
	}

	return return_val;
}

/**
 * start_command - set GO bit to send command to device
 * @param reg:	nand_ctlr structure
 */
static void start_command(struct nand_ctlr *reg)
{
	u32 reg_val;

	reg_val = readl(&reg->command);
	reg_val |= CMD_GO;
	writel(reg_val, &reg->command);
}

/**
 * stop_command - clear command GO bit, DMA GO bit, and DMA completion status
 * @param reg:	nand_ctlr structure
 */
static void stop_command(struct nand_ctlr *reg)
{
	/* Stop command */
	writel(0, &reg->command);

	/* Stop DMA engine and clear DMA completion status */
	writel(DMA_MST_CTRL_GO_DISABLE
		| DMA_MST_CTRL_IS_DMA_DONE,
		&reg->dma_mst_ctrl);
}

/*
 * set_bus_width_page_size - set up NAND bus width and page size
 * @param nand_config:	nand_info structure
 * @param *reg_val: address of reg_val
 * @return: value is set in reg_val
 */
static void set_bus_width_page_size(struct nand_info *nand_config,
	u32 *reg_val)
{
	if (nand_config->bus_width == 8)
		*reg_val = CFG_BUS_WIDTH_8BIT;
	else
		*reg_val = CFG_BUS_WIDTH_16BIT;

	if (nand_config->page_data_bytes == 256)
		*reg_val |= CFG_PAGE_SIZE_256;
	else if (nand_config->page_data_bytes == 512)
		*reg_val |= CFG_PAGE_SIZE_512;
	else if (nand_config->page_data_bytes == 1024)
		*reg_val |= CFG_PAGE_SIZE_1024;
	else if (nand_config->page_data_bytes == 2048)
		*reg_val |= CFG_PAGE_SIZE_2048;
}

/**
 * nand_rw_page - page read/write function
 * @param mtd:	mtd info structure
 * @param chip:	nand chip info structure
 * @param buf:	data buffer
 * @param page:	page number
 * @param with_ecc:	1 to enable ECC, 0 to disable ECC
 * @param is_writing:	0 for read, 1 for write
 * @return:	0 when successfully completed
 *		-EIO when command timeout
 */
static int nand_rw_page(struct mtd_info *mtd, struct nand_chip *chip,
	uint8_t *buf, int page, int with_ecc, int is_writing)
{
	u32 reg_val;
	int tag_size;
	struct nand_oobfree *free = chip->ecc.layout->oobfree;
	/* 128 is larger than the value that our HW can support. */
	u32 tag_buf[128];
	char *tag_ptr;
	struct nand_info *nand_config;

	if (((int) buf) & 0x03) {
		printf("buf 0x%X has to be 4-byte aligned\n", (u32) buf);
		return -EINVAL;
	}

	nand_config = (struct nand_info *) chip->priv;

	/* Need to be 4-byte aligned */
	tag_ptr = (char *) &tag_buf;

	stop_command(nand_config->reg);

	writel((1 << chip->page_shift) - 1, &nand_config->reg->dma_cfg_a);
	writel((u32) buf, &nand_config->reg->data_block_ptr);

	if (with_ecc) {
		writel((u32) tag_ptr, &nand_config->reg->tag_ptr);
		if (is_writing)
			memcpy(tag_ptr, chip->oob_poi + free->offset,
				nand_config->tag_bytes +
				nand_config->tag_ecc_bytes);
	} else
		writel((u32) chip->oob_poi, &nand_config->reg->tag_ptr);

	set_bus_width_page_size(nand_config, &reg_val);

	/* Set ECC selection, configure ECC settings */
	if (with_ecc) {
		tag_size = nand_config->tag_bytes + nand_config->tag_ecc_bytes;
		reg_val |= (CFG_SKIP_SPARE_SEL_4
			| CFG_SKIP_SPARE_ENABLE
			| CFG_HW_ECC_CORRECTION_ENABLE
			| CFG_ECC_EN_TAG_DISABLE
			| CFG_HW_ECC_SEL_RS
			| CFG_HW_ECC_ENABLE
			| CFG_TVAL4
			| (tag_size - 1));

		if (!is_writing) {
			tag_size += nand_config->skipped_spare_bytes;
			invalidate_dcache_range((unsigned long) tag_ptr,
				((unsigned long) tag_ptr) + tag_size);
		} else
			flush_dcache_range((unsigned long) tag_ptr,
				((unsigned long) tag_ptr) + tag_size);
	} else {
		tag_size = mtd->oobsize;
		reg_val |= (CFG_SKIP_SPARE_DISABLE
			| CFG_HW_ECC_CORRECTION_DISABLE
			| CFG_ECC_EN_TAG_DISABLE
			| CFG_HW_ECC_DISABLE
			| (tag_size - 1));
		if (!is_writing) {
			invalidate_dcache_range((unsigned long) chip->oob_poi,
				((unsigned long) chip->oob_poi) + tag_size);
		} else {
			flush_dcache_range((unsigned long) chip->oob_poi,
				((unsigned long) chip->oob_poi) + tag_size);
		}
	}
	writel(reg_val, &nand_config->reg->config);

	if (!is_writing) {
		invalidate_dcache_range((unsigned long) buf,
			((unsigned long) buf) +
			(1 << chip->page_shift));
	} else {
		flush_dcache_range((unsigned long) buf,
			((unsigned long) buf) +
			(1 << chip->page_shift));
	}

	writel(BCH_CONFIG_BCH_ECC_DISABLE, &nand_config->reg->bch_config);

	writel(tag_size - 1, &nand_config->reg->dma_cfg_b);

	nand_clear_interrupt_status(nand_config->reg);

	reg_val = CMD_CLE | CMD_ALE
		| CMD_SEC_CMD
		| (CMD_ALE_BYTES5 << CMD_ALE_BYTE_SIZE_SHIFT)
		| CMD_A_VALID
		| CMD_B_VALID
		| (CMD_TRANS_SIZE_BYTES_PAGE_SIZE_SEL <<
		CMD_TRANS_SIZE_SHIFT)
		| CMD_CE0;
	if (!is_writing)
		reg_val |= (CMD_AFT_DAT_DISABLE | CMD_RX);
	else
		reg_val |= (CMD_AFT_DAT_ENABLE | CMD_TX);
	writel(reg_val, &nand_config->reg->command);

	/* Setup DMA engine */
	reg_val = DMA_MST_CTRL_GO_ENABLE
		| DMA_MST_CTRL_BURST_8WORDS
		| DMA_MST_CTRL_EN_A_ENABLE
		| DMA_MST_CTRL_EN_B_ENABLE;

	if (!is_writing)
		reg_val |= DMA_MST_CTRL_DIR_READ;
	else
		reg_val |= DMA_MST_CTRL_DIR_WRITE;

	writel(reg_val, &nand_config->reg->dma_mst_ctrl);

	start_command(nand_config->reg);

	if (!nand_waitfor_cmd_completion(nand_config->reg)) {
		if (!is_writing)
			printf("Read Page 0x%X timeout ", page);
		else
			printf("Write Page 0x%X timeout ", page);
		if (with_ecc)
			printf("with ECC");
		else
			printf("without ECC");
		printf("\n");
		return -EIO;
	}

	if (with_ecc && !is_writing) {
		memcpy(chip->oob_poi, tag_ptr,
			nand_config->skipped_spare_bytes);
		memcpy(chip->oob_poi + free->offset,
			tag_ptr + nand_config->skipped_spare_bytes,
			nand_config->tag_bytes);
		reg_val = (u32) check_ecc_error(nand_config->reg, (u8 *) buf,
			1 << chip->page_shift,
			(u8 *) (tag_ptr + nand_config->skipped_spare_bytes),
			nand_config->tag_bytes);
		if (reg_val & ECC_TAG_ERROR)
			printf("Read Page 0x%X tag ECC error\n", page);
		if (reg_val & ECC_DATA_ERROR)
			printf("Read Page 0x%X data ECC error\n",
				page);
		if (reg_val & (ECC_DATA_ERROR | ECC_TAG_ERROR))
			return -EIO;
	}
	return 0;
}

/**
 * nand_read_page_hwecc - hardware ecc based page read function
 * @param mtd:	mtd info structure
 * @param chip:	nand chip info structure
 * @param buf:	buffer to store read data
 * @param page:	page number to read
 * @return:	0 when successfully completed
 *		-EIO when command timeout
 */
static int nand_read_page_hwecc(struct mtd_info *mtd,
	struct nand_chip *chip, uint8_t *buf, int page)
{
	return nand_rw_page(mtd, chip, buf, page, 1, 0);
}

/**
 * nand_write_page_hwecc - hardware ecc based page write function
 * @param mtd:	mtd info structure
 * @param chip:	nand chip info structure
 * @param buf:	data buffer
 */
static void nand_write_page_hwecc(struct mtd_info *mtd,
	struct nand_chip *chip, const uint8_t *buf)
{
	int page;
	struct nand_info *nand_config;

	nand_config = (struct nand_info *) chip->priv;

	page = (readl(&nand_config->reg->addr_reg1) >> 16) |
		(readl(&nand_config->reg->addr_reg2) << 16);

	nand_rw_page(mtd, chip, (uint8_t *) buf, page, 1, 1);
}


/**
 * nand_read_page_raw - read raw page data without ecc
 * @param mtd:	mtd info structure
 * @param chip:	nand chip info structure
 * @param buf:	buffer to store read data
 * @param page:	page number to read
 * @return:	0 when successfully completed
 *		-EINVAL when chip->oob_poi is not double-word aligned
 *		-EIO when command timeout
 */
static int nand_read_page_raw(struct mtd_info *mtd,
	struct nand_chip *chip, uint8_t *buf, int page)
{
	return nand_rw_page(mtd, chip, buf, page, 0, 0);
}

/**
 * nand_write_page_raw - raw page write function
 * @param mtd:	mtd info structure
 * @param chip:	nand chip info structure
 * @param buf:	data buffer
 */
static void nand_write_page_raw(struct mtd_info *mtd,
		struct nand_chip *chip,	const uint8_t *buf)
{
	int page;
	struct nand_info *nand_config;

	nand_config = (struct nand_info *) chip->priv;
	page = (readl(&nand_config->reg->addr_reg1) >> 16) |
		(readl(&nand_config->reg->addr_reg2) << 16);

	nand_rw_page(mtd, chip, (uint8_t *) buf, page, 0, 1);
}

/**
 * nand_rw_oob - OOB data read/write function
 * @param mtd:	mtd info structure
 * @param chip:	nand chip info structure
 * @param page:	page number to read
 * @param with_ecc:	1 to enable ECC, 0 to disable ECC
 * @param is_writing:	0 for read, 1 for write
 * @return:	0 when successfully completed
 *		-EINVAL when chip->oob_poi is not double-word aligned
 *		-EIO when command timeout
 */
static int nand_rw_oob(struct mtd_info *mtd, struct nand_chip *chip,
	int page, int with_ecc, int is_writing)
{
	u32 reg_val;
	int tag_size;
	struct nand_oobfree *free = chip->ecc.layout->oobfree;
	struct nand_info *nand_config;

	if (((int) chip->oob_poi) & 0x03)
		return -EINVAL;

	nand_config = (struct nand_info *) chip->priv;
	stop_command(nand_config->reg);

	writel((u32) chip->oob_poi, &nand_config->reg->tag_ptr);

	set_bus_width_page_size(nand_config, &reg_val);

	/* Set ECC selection */
	tag_size = mtd->oobsize;
	if (with_ecc)
		reg_val |= CFG_ECC_EN_TAG_ENABLE;
	else
		reg_val |= (CFG_ECC_EN_TAG_DISABLE);

	reg_val |= ((tag_size - 1) |
		CFG_SKIP_SPARE_DISABLE |
		CFG_HW_ECC_CORRECTION_DISABLE |
		CFG_HW_ECC_DISABLE);
	writel(reg_val, &nand_config->reg->config);

	if (!is_writing)
		invalidate_dcache_range((unsigned long) chip->oob_poi,
			((unsigned long) chip->oob_poi) + tag_size);
	else
		flush_dcache_range((unsigned long) chip->oob_poi,
			((unsigned long) chip->oob_poi) + tag_size);

	writel(BCH_CONFIG_BCH_ECC_DISABLE, &nand_config->reg->bch_config);

	if (is_writing && with_ecc)
		tag_size -= nand_config->tag_ecc_bytes;

	writel(tag_size - 1, &nand_config->reg->dma_cfg_b);

	nand_clear_interrupt_status(nand_config->reg);

	reg_val = CMD_CLE | CMD_ALE
		| CMD_SEC_CMD
		| (CMD_ALE_BYTES5 << CMD_ALE_BYTE_SIZE_SHIFT)
		| CMD_B_VALID
		| CMD_CE0;
	if (!is_writing)
		reg_val |= (CMD_AFT_DAT_DISABLE | CMD_RX);
	else
		reg_val |= (CMD_AFT_DAT_ENABLE | CMD_TX);
	writel(reg_val, &nand_config->reg->command);

	/* Setup DMA engine */
	reg_val = DMA_MST_CTRL_GO_ENABLE
		| DMA_MST_CTRL_BURST_8WORDS
		| DMA_MST_CTRL_EN_B_ENABLE;
	if (!is_writing)
		reg_val |= DMA_MST_CTRL_DIR_READ;
	else
		reg_val |= DMA_MST_CTRL_DIR_WRITE;

	writel(reg_val, &nand_config->reg->dma_mst_ctrl);

	start_command(nand_config->reg);

	if (!nand_waitfor_cmd_completion(nand_config->reg)) {
		if (!is_writing)
			printf("Read OOB of Page 0x%X timeout\n", page);
		else
			printf("Write OOB of Page 0x%X timeout\n", page);
		return -EIO;
	}

	if (with_ecc && !is_writing) {
		reg_val = (u32) check_ecc_error(nand_config->reg, 0, 0,
			(u8 *) (chip->oob_poi + free->offset),
			nand_config->tag_bytes);
		if (reg_val & ECC_TAG_ERROR)
			printf("Read OOB of Page 0x%X tag ECC error\n", page);
	}
	return 0;
}

/**
 * nand_read_oob - OOB data read function
 * @param mtd:	mtd info structure
 * @param chip:	nand chip info structure
 * @param page:	page number to read
 * @param sndcmd:	flag whether to issue read command or not
 * @return:	1 - issue read command next time
 *		0 - not to issue
 */
static int nand_read_oob(struct mtd_info *mtd, struct nand_chip *chip,
	int page, int sndcmd)
{
	if (sndcmd) {
		chip->cmdfunc(mtd, NAND_CMD_READOOB, 0, page);
		sndcmd = 0;
	}
	nand_rw_oob(mtd, chip, page, 0, 0);
	return sndcmd;
}

/**
 * nand_write_oob - OOB data write function
 * @param mtd:	mtd info structure
 * @param chip:	nand chip info structure
 * @param page:	page number to write
 * @return:	0 when successfully completed
 *		-EINVAL when chip->oob_poi is not double-word aligned
 *		-EIO when command timeout
 */
static int nand_write_oob(struct mtd_info *mtd, struct nand_chip *chip,
	int page)
{
	chip->cmdfunc(mtd, NAND_CMD_SEQIN, mtd->writesize, page);

	return nand_rw_oob(mtd, chip, page, 0, 1);
}

/*
 * Board-specific NAND initialization.
 * @param nand:	nand chip info structure
 * @return: 0, after initialized.
 */
int board_nand_init(struct nand_chip *nand)
{
	struct nand_info *nand_config = &nand_ctrl;
	u32 reg_val, clk_rate, clk_period;

	/* get configuration settings from header file */
	nand_config->reg = (struct nand_ctlr *) CONFIG_SYS_NAND_BASE;
	nand_config->bus_width = CONFIG_NAND_BUS_WIDTH;
	nand_config->page_data_bytes = CONFIG_NAND_PAGE_DATA_BYTES;
	nand_config->page_spare_bytes = CONFIG_NAND_PAGE_SPARE_BYTES;
	nand_config->skipped_spare_bytes = CONFIG_NAND_SKIPPED_SPARE_BYTES;
	nand_config->rs_data_ecc_bytes = CONFIG_NAND_RS_DATA_ECC_BYTES;
	nand_config->tag_bytes = CONFIG_NAND_TAG_BYTES;
	nand_config->tag_ecc_bytes = CONFIG_NAND_TAG_ECC_BYTES;

	/* Adjust controller clock rate */
	clock_start_periph_pll(PERIPH_ID_NDFLASH, CLOCK_ID_PERIPH, CLK_52M);

	/* Adjust timing for NAND device */
	clk_rate = (u32) clock_get_periph_rate(PERIPH_ID_NDFLASH,
		CLOCK_ID_PERIPH) / 1000000;
	clk_period = 1000 / clk_rate;
	reg_val = ((CONFIG_NAND_MAX_TRP_TREA / clk_period) <<
		TIMING_TRP_RESP_CNT_SHIFT) & TIMING_TRP_RESP_CNT_MASK;
	reg_val |= ((CONFIG_NAND_TWB / clk_period) <<
		TIMING_TWB_CNT_SHIFT) & TIMING_TWB_CNT_MASK;
	if ((CONFIG_NAND_MAX_TCR_TAR_TRR / clk_period) > 2)
		reg_val |= (((CONFIG_NAND_MAX_TCR_TAR_TRR / clk_period) - 2)
		<< TIMING_TCR_TAR_TRR_CNT_SHIFT) &
		TIMING_TCR_TAR_TRR_CNT_MASK;
	reg_val |= ((CONFIG_NAND_TWHR / clk_period) <<
		TIMING_TWHR_CNT_SHIFT) & TIMING_TWHR_CNT_MASK;
	if ((CONFIG_NAND_MAX_TCS_TCH_TALS_TALH / clk_period) > 1)
		reg_val |= (((CONFIG_NAND_MAX_TCS_TCH_TALS_TALH / clk_period)
		- 1) << TIMING_TCS_CNT_SHIFT) & TIMING_TCS_CNT_MASK;
	reg_val |= ((CONFIG_NAND_TWH / clk_period) <<
		TIMING_TWH_CNT_SHIFT) & TIMING_TWH_CNT_MASK;
	reg_val |= ((CONFIG_NAND_TWP / clk_period) <<
		TIMING_TWP_CNT_SHIFT) & TIMING_TWP_CNT_MASK;
	reg_val |= ((CONFIG_NAND_TRH / clk_period) <<
		TIMING_TRH_CNT_SHIFT) & TIMING_TRH_CNT_MASK;
	reg_val |= ((CONFIG_NAND_MAX_TRP_TREA / clk_period) <<
		TIMING_TRP_CNT_SHIFT) & TIMING_TRP_CNT_MASK;
	writel(reg_val, &nand_config->reg->timing);

	reg_val = 0;
	if ((CONFIG_NAND_TADL / clk_period) > 2)
		reg_val = ((CONFIG_NAND_TADL / clk_period) - 2) &
		TIMING2_TADL_CNT_MASK;
	writel(reg_val, &nand_config->reg->timing2);

	/* Pinmux ATC_SEL uses NAND */
	pinmux_set_func(PINGRP_ATC, PMUX_FUNC_NAND);

	nand_config->wp_gpio = CONFIG_NAND_WP_GPIO;
	gpio_direction_output(nand_config->wp_gpio, 1);

	nand->cmd_ctrl = nand_hwcontrol;
	nand->dev_ready  = nand_dev_ready;

	nand->ecc.mode = NAND_ECC_HW;
	nand->ecc.layout = &eccoob;
	nand->ecc.size = CONFIG_NAND_PAGE_DATA_BYTES;
	nand->ecc.bytes = CONFIG_NAND_PAGE_SPARE_BYTES;

	nand->options = LP_OPTIONS;
	nand->cmdfunc = nand_command;
	nand->read_byte = nand_read_byte;
	nand->read_buf = nand_read_buf;
	nand->write_buf = nand_write_buf;
	nand->ecc.read_page = nand_read_page_hwecc;
	nand->ecc.write_page = nand_write_page_hwecc;
	nand->ecc.read_page_raw = nand_read_page_raw;
	nand->ecc.write_page_raw = nand_write_page_raw;
	nand->ecc.read_oob = nand_read_oob;
	nand->ecc.write_oob = nand_write_oob;
	nand->priv = &nand_ctrl;
	return 0;
}
