enum {
	Bit0 = 1 << 0,
	Bit1 = 1 << 1,
	Bit2 = 1 << 2,
	Bit3 = 1 << 3,
	Bit4 = 1 << 4,
	Bit5 = 1 << 5,
	Bit6 = 1 << 6,
	Bit7 = 1 << 7,
	Bit8 = 1 << 8,
	Bit9 = 1 << 9,
	Bit10 = 1 << 10,
	Bit11 = 1 << 11,
	Bit12 = 1 << 12,
	Bit13 = 1 << 13,
	Bit14 = 1 << 14,
	Bit15 = 1 << 15,
	Bit16 = 1 << 16,
	Bit17 = 1 << 17,
	Bit18 = 1 << 18,
	Bit19 = 1 << 19,
	Bit20 = 1 << 20,
	Bit21 = 1 << 21,
	Bit22 = 1 << 22,
	Bit23 = 1 << 23,
	Bit24 = 1 << 24,
	Bit25 = 1 << 25,
	Bit26 = 1 << 26,
	Bit27 = 1 << 27,
	Bit28 = 1 << 28,
	Bit29 = 1 << 29,
	Bit30 = 1 << 30,
	Bit31 = 1 << 31
};

/* register offset */
#define COMMAND_0		0x00
#define CMD_GO		Bit31
#define CMD_CLE		Bit30
#define CMD_ALE		Bit29
#define CMD_PIO		Bit28
#define CMD_TX		Bit27
#define CMD_RX		Bit26
#define CMD_SEC_CMD	Bit25
#define CMD_AFT_DAT_MASK	Bit24
#define CMD_AFT_DAT_DISABLE	0
#define CMD_AFT_DAT_ENABLE	Bit24
#define CMD_TRANS_SIZE_SHIFT	20
enum {
	CMD_TRANS_SIZE_BYTES1 = 0,
	CMD_TRANS_SIZE_BYTES2,
	CMD_TRANS_SIZE_BYTES3,
	CMD_TRANS_SIZE_BYTES4,
	CMD_TRANS_SIZE_BYTES5,
	CMD_TRANS_SIZE_BYTES6,
	CMD_TRANS_SIZE_BYTES7,
	CMD_TRANS_SIZE_BYTES8,
	CMD_TRANS_SIZE_BYTES_PAGE_SIZE_SEL
};

#define CMD_TRANS_SIZE_BYTES_PAGE_SIZE_SEL	8
#define CMD_A_VALID	Bit19
#define CMD_B_VALID	Bit18
#define CMD_RD_STATUS_CHK	Bit17
#define CMD_R_BSY_CHK	Bit16
#define CMD_CE7		Bit15
#define CMD_CE6		Bit14
#define CMD_CE5		Bit13
#define CMD_CE4		Bit12
#define CMD_CE3		Bit11
#define CMD_CE2		Bit10
#define CMD_CE1		Bit9
#define CMD_CE0		Bit8
#define CMD_CLE_BYTE_SIZE_SHIFT	4
enum {
	CMD_CLE_BYTES1 = 0,
	CMD_CLE_BYTES2,
	CMD_CLE_BYTES3,
	CMD_CLE_BYTES4,
};
#define CMD_ALE_BYTE_SIZE_SHIFT	0
enum {
	CMD_ALE_BYTES1 = 0,
	CMD_ALE_BYTES2,
	CMD_ALE_BYTES3,
	CMD_ALE_BYTES4,
	CMD_ALE_BYTES5,
	CMD_ALE_BYTES6,
	CMD_ALE_BYTES7,
	CMD_ALE_BYTES8
};

#define STATUS_0		0x04
#define STATUS_RBSY0	Bit8

#define ISR_0			0x08
#define ISR_IS_CMD_DONE	Bit5
#define ISR_IS_ECC_ERR	Bit4

#define IER_0			0x0C

#define CFG_0		0x10
#define CFG_HW_ECC_MASK		Bit31
#define CFG_HW_ECC_DISABLE		0
#define CFG_HW_ECC_ENABLE		Bit31
#define CFG_HW_ECC_SEL_MASK		Bit30
#define CFG_HW_ECC_SEL_HAMMING	0
#define CFG_HW_ECC_SEL_RS		Bit30
#define CFG_HW_ECC_CORRECTION_MASK	Bit29
#define CFG_HW_ECC_CORRECTION_DISABLE	0
#define CFG_HW_ECC_CORRECTION_ENABLE	Bit29
#define CFG_PIPELINE_EN_MASK		Bit28
#define CFG_PIPELINE_EN_DISABLE	0
#define CFG_PIPELINE_EN_ENABLE	Bit28
#define CFG_ECC_EN_TAG_MASK		Bit27
#define CFG_ECC_EN_TAG_DISABLE	0
#define CFG_ECC_EN_TAG_ENABLE	Bit27
#define CFG_TVALUE_MASK		(Bit25 | Bit24)
enum {
	CFG_TVAL4 = 0 << 24,
	CFG_TVAL6 = 1 << 24,
	CFG_TVAL8 = 2 << 24
};
#define CFG_SKIP_SPARE_MASK		Bit23
#define CFG_SKIP_SPARE_DISABLE	0
#define CFG_SKIP_SPARE_ENABLE	Bit23
#define CFG_COM_BSY_MASK		Bit22
#define CFG_COM_BSY_DISABLE		0
#define CFG_COM_BSY_ENABLE		Bit22
#define CFG_BUS_WIDTH_MASK		Bit21
#define CFG_BUS_WIDTH_8BIT		0
#define CFG_BUS_WIDTH_16BIT		Bit21
#define CFG_LPDDR1_MODE_MASK		Bit20
#define CFG_LPDDR1_MODE_DISABLE	0
#define CFG_LPDDR1_MODE_ENABLE	Bit20
#define CFG_EDO_MODE_MASK		Bit19
#define CFG_EDO_MODE_DISABLE		0
#define CFG_EDO_MODE_ENABLE		Bit19
#define CFG_PAGE_SIZE_SEL_MASK	(Bit18 | Bit17 | Bit16)
enum {
	CFG_PAGE_SIZE_256	= 0 << 16,
	CFG_PAGE_SIZE_512	= 1 << 16,
	CFG_PAGE_SIZE_1024	= 2 << 16,
	CFG_PAGE_SIZE_2048	= 3 << 16,
	CFG_PAGE_SIZE_4096	= 4 << 16
};
#define CFG_SKIP_SPARE_SEL_MASK		(Bit15 | Bit14)
enum {
	CFG_SKIP_SPARE_SEL_4		= 0 << 14,
	CFG_SKIP_SPARE_SEL_8		= 1 << 14,
	CFG_SKIP_SPARE_SEL_12	= 2 << 14,
	CFG_SKIP_SPARE_SEL_16	= 3 << 14
};
#define CFG_TAG_BYTE_SIZE_MASK	0x1FF

#define TIMING_0		0x14
#define TIMING_TRP_RESP_CNT_SHIFT	28
#define TIMING_TRP_RESP_CNT_MASK	(Bit31 | Bit30 | Bit29 | Bit28)
#define TIMING_TWB_CNT_SHIFT		24
#define TIMING_TWB_CNT_MASK		(Bit27 | Bit26 | Bit25 | Bit24)
#define TIMING_TCR_TAR_TRR_CNT_SHIFT	20
#define TIMING_TCR_TAR_TRR_CNT_MASK	(Bit23 | Bit22 | Bit21 | Bit20)
#define TIMING_TWHR_CNT_SHIFT		16
#define TIMING_TWHR_CNT_MASK		(Bit19 | Bit18 | Bit17 | Bit16)
#define TIMING_TCS_CNT_SHIFT		14
#define TIMING_TCS_CNT_MASK		(Bit15 | Bit14)
#define TIMING_TWH_CNT_SHIFT		12
#define TIMING_TWH_CNT_MASK		(Bit13 | Bit12)
#define TIMING_TWP_CNT_SHIFT		8
#define TIMING_TWP_CNT_MASK		(Bit11 | Bit10 | Bit9 | Bit8)
#define TIMING_TRH_CNT_SHIFT		4
#define TIMING_TRH_CNT_MASK		(Bit5 | Bit4)
#define TIMING_TRP_CNT_SHIFT		0
#define TIMING_TRP_CNT_MASK		(Bit3 | Bit2 | Bit1 | Bit0)

#define RESP_0			0x18

#define TIMING2_0		0x1C
#define TIMING2_TADL_CNT_SHIFT		0
#define TIMING2_TADL_CNT_MASK		(Bit3 | Bit2 | Bit1 | Bit0)

#define CMD_REG1_0		0x20
#define CMD_REG2_0		0x24
#define ADDR_REG1_0		0x28
#define ADDR_REG2_0		0x2C

#define DMA_MST_CTRL_0		0x30
#define DMA_MST_CTRL_GO_MASK		Bit31
#define DMA_MST_CTRL_GO_DISABLE		0
#define DMA_MST_CTRL_GO_ENABLE		Bit31
#define DMA_MST_CTRL_DIR_MASK		Bit30
#define DMA_MST_CTRL_DIR_READ		0
#define DMA_MST_CTRL_DIR_WRITE		Bit30
#define DMA_MST_CTRL_PERF_EN_MASK	Bit29
#define DMA_MST_CTRL_PERF_EN_DISABLE	0
#define DMA_MST_CTRL_PERF_EN_ENABLE	Bit29
#define DMA_MST_CTRL_REUSE_BUFFER_MASK	Bit27
#define DMA_MST_CTRL_REUSE_BUFFER_DISABLE	0
#define DMA_MST_CTRL_REUSE_BUFFER_ENABLE	Bit27
#define DMA_MST_CTRL_BURST_SIZE_MASK	(Bit26 | Bit25 | Bit24)
enum {
	DMA_MST_CTRL_BURST_1WORDS	= 2 << 24,
	DMA_MST_CTRL_BURST_4WORDS	= 3 << 24,
	DMA_MST_CTRL_BURST_8WORDS	= 4 << 24,
	DMA_MST_CTRL_BURST_16WORDS	= 5 << 24
};
#define DMA_MST_CTRL_IS_DMA_DONE	Bit20
#define DMA_MST_CTRL_EN_A_MASK		Bit2
#define DMA_MST_CTRL_EN_A_DISABLE	0
#define DMA_MST_CTRL_EN_A_ENABLE	Bit2
#define DMA_MST_CTRL_EN_B_MASK		Bit1
#define DMA_MST_CTRL_EN_B_DISABLE	0
#define DMA_MST_CTRL_EN_B_ENABLE	Bit1

#define DMA_CFG_A_0		0x34
#define DMA_CFG_B_0		0x38
#define FIFO_CTRL_0		0x3C
#define DATA_BLOCK_PTR_0	0x40
#define TAG_PTR_0		0x44
#define ECC_PTR_0		0x48

#define DEC_STATUS_0		0x4C
#define DEC_STATUS_A_ECC_FAIL		Bit1
#define DEC_STATUS_B_ECC_FAIL		Bit0

#define BCH_CONFIG_0		0xCC
#define BCH_CONFIG_BCH_TVALUE_MASK	(Bit5 | Bit4)
enum {
	BCH_CONFIG_BCH_TVAL4	= 0 << 4,
	BCH_CONFIG_BCH_TVAL8	= 1 << 4,
	BCH_CONFIG_BCH_TVAL14	= 2 << 4,
	BCH_CONFIG_BCH_TVAL16	= 3 << 4
};
#define BCH_CONFIG_BCH_ECC_MASK		Bit0
#define BCH_CONFIG_BCH_ECC_DISABLE	0
#define BCH_CONFIG_BCH_ECC_ENABLE	Bit0

#define BCH_DEC_RESULT_0	0xD0
#define BCH_DEC_RESULT_CORRFAIL_ERR_MASK	Bit8
#define BCH_DEC_RESULT_PAGE_COUNT_MASK		0xFF

#define BCH_DEC_STATUS_BUF_0	0xD4
#define BCH_DEC_STATUS_FAIL_SEC_FLAG_MASK	0xFF000000
#define BCH_DEC_STATUS_CORR_SEC_FLAG_MASK	0x00FF0000
#define BCH_DEC_STATUS_FAIL_TAG_MASK		Bit14
#define BCH_DEC_STATUS_CORR_TAG_MASK		Bit13
#define BCH_DEC_STATUS_MAX_CORR_CNT_MASK (Bit12 | Bit11 | Bit10 | Bit9 | Bit8)
#define BCH_DEC_STATUS_PAGE_NUMBER_MASK		0xFF

#define LP_OPTIONS (NAND_NO_READRDY | NAND_NO_AUTOINCR)

struct nand_ctlr {
	u32	command;	/* offset 00h */
	u32	status;		/* offset 04h */
	u32	isr;		/* offset 08h */
	u32	ier;		/* offset 0Ch */
	u32	config;		/* offset 10h */
	u32	timing;		/* offset 14h */
	u32	resp;		/* offset 18h */
	u32	timing2;	/* offset 1Ch */
	u32	cmd_reg1;	/* offset 20h */
	u32	cmd_reg2;	/* offset 24h */
	u32	addr_reg1;	/* offset 28h */
	u32	addr_reg2;	/* offset 2Ch */
	u32	dma_mst_ctrl;	/* offset 30h */
	u32	dma_cfg_a;	/* offset 34h */
	u32	dma_cfg_b;	/* offset 38h */
	u32	fifo_ctrl;	/* offset 3Ch */
	u32	data_block_ptr;	/* offset 40h */
	u32	tag_ptr;	/* offset 44h */
	u32	resv1;		/* offset 48h */
	u32	dec_status;	/* offset 4Ch */
	u32	hwstatus_cmd;	/* offset 50h */
	u32	hwstatus_mask;	/* offset 54h */
	u32	resv2[29];
	u32	bch_config;	/* offset CCh */
	u32	bch_dec_result;	/* offset D0h */
	u32	bch_dec_status_buf;
				/* offset D4h */
};
