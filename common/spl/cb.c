#include <common.h>
#include <asm/io.h>
#include <spl.h>

#define EL0               0
#define EL1               1
#define EL2               2
#define EL3               3

#define CURRENT_EL_MASK   0x3
#define CURRENT_EL_SHIFT  2

#define SPSR_USE_L           0
#define SPSR_USE_H           1
#define SPSR_L_H_MASK        1
#define SPSR_M_SHIFT         4
#define SPSR_ERET_32         (1 << SPSR_M_SHIFT)
#define SPSR_ERET_64         (0 << SPSR_M_SHIFT)
#define SPSR_FIQ             (1 << 6)
#define SPSR_IRQ             (1 << 7)
#define SPSR_SERROR          (1 << 8)
#define SPSR_DEBUG           (1 << 9)
#define SPSR_EXCEPTION_MASK  (SPSR_FIQ | SPSR_IRQ | SPSR_SERROR | SPSR_DEBUG)

#define SCR_NS_SHIFT         0
#define SCR_NS_MASK          (1 << SCR_NS_SHIFT)
#define SCR_NS_ENABLE        (1 << SCR_NS_SHIFT)
#define SCR_NS_DISABLE       (0 << SCR_NS_SHIFT)
#define SCR_NS               SCR_NS_ENABLE
#define SCR_RES1             (0x3 << 4)
#define SCR_IRQ_SHIFT        2
#define SCR_IRQ_MASK         (1 << SCR_IRQ_SHIFT)
#define SCR_IRQ_ENABLE       (1 << SCR_IRQ_SHIFT)
#define SCR_IRQ_DISABLE      (0 << SCR_IRQ_SHIFT)
#define SCR_FIQ_SHIFT        2
#define SCR_FIQ_MASK         (1 << SCR_FIQ_SHIFT)
#define SCR_FIQ_ENABLE       (1 << SCR_FIQ_SHIFT)
#define SCR_FIQ_DISABLE      (0 << SCR_FIQ_SHIFT)
#define SCR_EA_SHIFT         3
#define SCR_EA_MASK          (1 << SCR_EA_SHIFT)
#define SCR_EA_ENABLE        (1 << SCR_EA_SHIFT)
#define SCR_EA_DISABLE       (0 << SCR_EA_SHIFT)
#define SCR_SMD_SHIFT        7
#define SCR_SMD_MASK         (1 << SCR_SMD_SHIFT)
#define SCR_SMD_DISABLE      (1 << SCR_SMD_SHIFT)
#define SCR_SMD_ENABLE       (0 << SCR_SMD_SHIFT)
#define SCR_HVC_SHIFT        8
#define SCR_HVC_MASK         (1 << SCR_HVC_SHIFT)
#define SCR_HVC_DISABLE      (0 << SCR_HVC_SHIFT)
#define SCR_HVC_ENABLE       (1 << SCR_HVC_SHIFT)
#define SCR_SIF_SHIFT        9
#define SCR_SIF_MASK         (1 << SCR_SIF_SHIFT)
#define SCR_SIF_ENABLE       (1 << SCR_SIF_SHIFT)
#define SCR_SIF_DISABLE      (0 << SCR_SIF_SHIFT)
#define SCR_RW_SHIFT         10
#define SCR_RW_MASK          (1 << SCR_RW_SHIFT)
#define SCR_LOWER_AARCH64    (1 << SCR_RW_SHIFT)
#define SCR_LOWER_AARCH32    (0 << SCR_RW_SHIFT)
#define SCR_ST_SHIFT         11
#define SCR_ST_MASK          (1 << SCR_ST_SHIFT)
#define SCR_ST_ENABLE        (1 << SCR_ST_SHIFT)
#define SCR_ST_DISABLE       (0 << SCR_ST_SHIFT)
#define SCR_TWI_SHIFT        12
#define SCR_TWI_MASK         (1 << SCR_TWI_SHIFT)
#define SCR_TWI_ENABLE       (1 << SCR_TWI_SHIFT)
#define SCR_TWI_DISABLE      (0 << SCR_TWI_SHIFT)
#define SCR_TWE_SHIFT        13
#define SCR_TWE_MASK         (1 << SCR_TWE_SHIFT)
#define SCR_TWE_ENABLE       (1 << SCR_TWE_SHIFT)
#define SCR_TWE_DISABLE      (0 << SCR_TWE_SHIFT)

#define HCR_RW_SHIFT         31
#define HCR_LOWER_AARCH64    (1 << HCR_RW_SHIFT)
#define HCR_LOWER_AARCH32    (0 << HCR_RW_SHIFT)

#define SCTLR_MMU_ENABLE     1
#define SCTLR_MMU_DISABLE    0
#define SCTLR_ACE_SHIFT      1
#define SCTLR_ACE_ENABLE     (1 << SCTLR_ACE_SHIFT)
#define SCTLR_ACE_DISABLE    (0 << SCTLR_ACE_SHIFT)
#define SCTLR_CACHE_SHIFT    2
#define SCTLR_CACHE_ENABLE   (1 << SCTLR_CACHE_SHIFT)
#define SCTLR_CACHE_DISABLE  (0 << SCTLR_CACHE_SHIFT)
#define SCTLR_SAE_SHIFT      3
#define SCTLR_SAE_ENABLE     (1 << SCTLR_SAE_SHIFT)
#define SCTLR_SAE_DISABLE    (0 << SCTLR_SAE_SHIFT)
#define SCTLR_RES1           ((0x3 << 4) | (0x1 << 11) | (0x1 << 16) |	\
			      (0x1 << 18) | (0x3 << 22) | (0x3 << 28))
#define SCTLR_ICE_SHIFT      12
#define SCTLR_ICE_ENABLE     (1 << SCTLR_ICE_SHIFT)
#define SCTLR_ICE_DISABLE    (0 << SCTLR_ICE_SHIFT)
#define SCTLR_WXN_SHIFT      19
#define SCTLR_WXN_ENABLE     (1 << SCTLR_WXN_SHIFT)
#define SCTLR_WXN_DISABLE    (0 << SCTLR_WXN_SHIFT)
#define SCTLR_ENDIAN_SHIFT   25
#define SCTLR_LITTLE_END     (0 << SCTLR_ENDIAN_SHIFT)
#define SCTLR_BIG_END        (1 << SCTLR_ENDIAN_SHIFT)

#define CPTR_EL3_TCPAC_SHIFT	(31)
#define CPTR_EL3_TTA_SHIFT	(20)
#define CPTR_EL3_TFP_SHIFT	(10)
#define CPTR_EL3_TCPAC_DISABLE	(0 << CPTR_EL3_TCPAC_SHIFT)
#define CPTR_EL3_TCPAC_ENABLE	(1 << CPTR_EL3_TCPAC_SHIFT)
#define CPTR_EL3_TTA_DISABLE	(0 << CPTR_EL3_TTA_SHIFT)
#define CPTR_EL3_TTA_ENABLE	(1 << CPTR_EL3_TTA_SHIFT)
#define CPTR_EL3_TFP_DISABLE	(0 << CPTR_EL3_TFP_SHIFT)
#define CPTR_EL3_TFP_ENABLE	(1 << CPTR_EL3_TFP_SHIFT)

#define CPACR_TTA_SHIFT	(28)
#define CPACR_TTA_ENABLE	(1 << CPACR_TTA_SHIFT)
#define CPACR_TTA_DISABLE	(0 << CPACR_TTA_SHIFT)
#define CPACR_FPEN_SHIFT	(20)
/*
 * ARMv8-A spec: Values 0b00 and 0b10 both seem to enable traps from el0 and el1
 * for fp reg access.
 */
#define CPACR_TRAP_FP_EL0_EL1	(0 << CPACR_FPEN_SHIFT)
#define CPACR_TRAP_FP_EL0	(1 << CPACR_FPEN_SHIFT)
#define CPACR_TRAP_FP_DISABLE	(3 << CPACR_FPEN_SHIFT)

#define DAIF_DBG_BIT      (1<<3)
#define DAIF_ABT_BIT      (1<<2)
#define DAIF_IRQ_BIT      (1<<1)
#define DAIF_FIQ_BIT      (1<<0)

#define MAKE_REGISTER_ACCESSORS(reg) \
	static inline uint64_t raw_read_##reg(void) \
	{ \
		uint64_t value; \
		__asm__ __volatile__("mrs %0, " #reg "\n\t" \
				     : "=r" (value) : : "memory"); \
		return value; \
	} \
	static inline void raw_write_##reg(uint64_t value) \
	{ \
		__asm__ __volatile__("msr " #reg ", %0\n\t" \
				     : : "r" (value) : "memory"); \
	}

#define MAKE_REGISTER_ACCESSORS_EL123(reg) \
	MAKE_REGISTER_ACCESSORS(reg##_el1) \
	MAKE_REGISTER_ACCESSORS(reg##_el2) \
	MAKE_REGISTER_ACCESSORS(reg##_el3)

/* Architectural register accessors */
MAKE_REGISTER_ACCESSORS_EL123(actlr)
MAKE_REGISTER_ACCESSORS_EL123(afsr0)
MAKE_REGISTER_ACCESSORS_EL123(afsr1)
MAKE_REGISTER_ACCESSORS(aidr_el1)
MAKE_REGISTER_ACCESSORS_EL123(amair)
MAKE_REGISTER_ACCESSORS(ccsidr_el1)
MAKE_REGISTER_ACCESSORS(clidr_el1)
MAKE_REGISTER_ACCESSORS(cntfrq_el0)
MAKE_REGISTER_ACCESSORS(cnthctl_el2)
MAKE_REGISTER_ACCESSORS(cnthp_ctl_el2)
MAKE_REGISTER_ACCESSORS(cnthp_cval_el2)
MAKE_REGISTER_ACCESSORS(cnthp_tval_el2)
MAKE_REGISTER_ACCESSORS(cntkctl_el1)
MAKE_REGISTER_ACCESSORS(cntp_ctl_el0)
MAKE_REGISTER_ACCESSORS(cntp_cval_el0)
MAKE_REGISTER_ACCESSORS(cntp_tval_el0)
MAKE_REGISTER_ACCESSORS(cntpct_el0)
MAKE_REGISTER_ACCESSORS(cntps_ctl_el1)
MAKE_REGISTER_ACCESSORS(cntps_cval_el1)
MAKE_REGISTER_ACCESSORS(cntps_tval_el1)
MAKE_REGISTER_ACCESSORS(cntv_ctl_el0)
MAKE_REGISTER_ACCESSORS(cntv_cval_el0)
MAKE_REGISTER_ACCESSORS(cntv_tval_el0)
MAKE_REGISTER_ACCESSORS(cntvct_el0)
MAKE_REGISTER_ACCESSORS(cntvoff_el2)
MAKE_REGISTER_ACCESSORS(contextidr_el1)
MAKE_REGISTER_ACCESSORS(cpacr_el1)
MAKE_REGISTER_ACCESSORS(cptr_el2)
MAKE_REGISTER_ACCESSORS(cptr_el3)
MAKE_REGISTER_ACCESSORS(csselr_el1)
MAKE_REGISTER_ACCESSORS(ctr_el0)
MAKE_REGISTER_ACCESSORS(currentel)
MAKE_REGISTER_ACCESSORS(daif)
MAKE_REGISTER_ACCESSORS(dczid_el0)
MAKE_REGISTER_ACCESSORS_EL123(elr)
MAKE_REGISTER_ACCESSORS_EL123(esr)
MAKE_REGISTER_ACCESSORS_EL123(far)
MAKE_REGISTER_ACCESSORS(fpcr)
MAKE_REGISTER_ACCESSORS(fpsr)
MAKE_REGISTER_ACCESSORS(hacr_el2)
MAKE_REGISTER_ACCESSORS(hcr_el2)
MAKE_REGISTER_ACCESSORS(hpfar_el2)
MAKE_REGISTER_ACCESSORS(hstr_el2)
MAKE_REGISTER_ACCESSORS(isr_el1)
MAKE_REGISTER_ACCESSORS_EL123(mair)
MAKE_REGISTER_ACCESSORS(midr_el1)
MAKE_REGISTER_ACCESSORS(mpidr_el1)
MAKE_REGISTER_ACCESSORS(nzcv)
MAKE_REGISTER_ACCESSORS(par_el1)
MAKE_REGISTER_ACCESSORS(revdir_el1)
MAKE_REGISTER_ACCESSORS_EL123(rmr)
MAKE_REGISTER_ACCESSORS_EL123(rvbar)
MAKE_REGISTER_ACCESSORS(scr_el3)
MAKE_REGISTER_ACCESSORS_EL123(sctlr)
MAKE_REGISTER_ACCESSORS(sp_el0)
MAKE_REGISTER_ACCESSORS(sp_el1)
MAKE_REGISTER_ACCESSORS(sp_el2)
MAKE_REGISTER_ACCESSORS(spsel)
MAKE_REGISTER_ACCESSORS_EL123(spsr)
MAKE_REGISTER_ACCESSORS(spsr_abt)
MAKE_REGISTER_ACCESSORS(spsr_fiq)
MAKE_REGISTER_ACCESSORS(spsr_irq)
MAKE_REGISTER_ACCESSORS(spsr_und)
MAKE_REGISTER_ACCESSORS_EL123(tcr)
MAKE_REGISTER_ACCESSORS_EL123(tpidr)
MAKE_REGISTER_ACCESSORS_EL123(ttbr0)
MAKE_REGISTER_ACCESSORS(ttbr1_el1)
MAKE_REGISTER_ACCESSORS_EL123(vbar)
MAKE_REGISTER_ACCESSORS(vmpidr_el2)
MAKE_REGISTER_ACCESSORS(vpidr_el2)
MAKE_REGISTER_ACCESSORS(vtcr_el2)
MAKE_REGISTER_ACCESSORS(vttbr_el2)

/* Special DAIF accessor functions */
static inline void enable_debug_exceptions(void)
{
	__asm__ __volatile__("msr DAIFClr, %0\n\t"
			     : : "i" (DAIF_DBG_BIT)  : "memory");
}

static inline void enable_serror_exceptions(void)
{
	__asm__ __volatile__("msr DAIFClr, %0\n\t"
			     : : "i" (DAIF_ABT_BIT)  : "memory");
}

static inline void enable_irq(void)
{
	__asm__ __volatile__("msr DAIFClr, %0\n\t"
			     : : "i" (DAIF_IRQ_BIT)  : "memory");
}

static inline void enable_fiq(void)
{
	__asm__ __volatile__("msr DAIFClr, %0\n\t"
			     : : "i" (DAIF_FIQ_BIT)  : "memory");
}

static inline void disable_debug_exceptions(void)
{
	__asm__ __volatile__("msr DAIFSet, %0\n\t"
			     : : "i" (DAIF_DBG_BIT)  : "memory");
}

static inline void disable_serror_exceptions(void)
{
	__asm__ __volatile__("msr DAIFSet, %0\n\t"
			     : : "i" (DAIF_ABT_BIT)  : "memory");
}

static inline void disable_irq(void)
{
	__asm__ __volatile__("msr DAIFSet, %0\n\t"
			     : : "i" (DAIF_IRQ_BIT)  : "memory");
}

static inline void disable_fiq(void)
{
	__asm__ __volatile__("msr DAIFSet, %0\n\t"
			     : : "i" (DAIF_FIQ_BIT)  : "memory");
}

/* Cache maintenance system instructions */
static inline void dccisw(uint64_t cisw)
{
	__asm__ __volatile__("dc cisw, %0\n\t" : : "r" (cisw) : "memory");
}

static inline void dccivac(uint64_t civac)
{
	__asm__ __volatile__("dc civac, %0\n\t" : : "r" (civac) : "memory");
}

static inline void dccsw(uint64_t csw)
{
	__asm__ __volatile__("dc csw, %0\n\t" : : "r" (csw) : "memory");
}

static inline void dccvac(uint64_t cvac)
{
	__asm__ __volatile__("dc cvac, %0\n\t" : : "r" (cvac) : "memory");
}

static inline void dccvau(uint64_t cvau)
{
	__asm__ __volatile__("dc cvau, %0\n\t" : : "r" (cvau) : "memory");
}

static inline void dcisw(uint64_t isw)
{
	__asm__ __volatile__("dc isw, %0\n\t" : : "r" (isw) : "memory");
}

static inline void dcivac(uint64_t ivac)
{
	__asm__ __volatile__("dc ivac, %0\n\t" : : "r" (ivac) : "memory");
}

static inline void dczva(uint64_t zva)
{
	__asm__ __volatile__("dc zva, %0\n\t" : : "r" (zva) : "memory");
}

static inline void iciallu(void)
{
	__asm__ __volatile__("ic iallu\n\t" : : : "memory");
}

static inline void icialluis(void)
{
	__asm__ __volatile__("ic ialluis\n\t" : : : "memory");
}

static inline void icivau(uint64_t ivau)
{
	__asm__ __volatile__("ic ivau, %0\n\t" : : "r" (ivau) : "memory");
}

/* TLB maintenance instructions */
static inline void tlbiall_el1(void)
{
	__asm__ __volatile__("tlbi alle1\n\t" : : : "memory");
}

static inline void tlbiall_el2(void)
{
	__asm__ __volatile__("tlbi alle2\n\t" : : : "memory");
}

static inline void tlbiall_el3(void)
{
	__asm__ __volatile__("tlbi alle3\n\t" : : : "memory");
}

static inline void tlbiallis_el1(void)
{
	__asm__ __volatile__("tlbi alle1is\n\t" : : : "memory");
}

static inline void tlbiallis_el2(void)
{
	__asm__ __volatile__("tlbi alle2is\n\t" : : : "memory");
}

static inline void tlbiallis_el3(void)
{
	__asm__ __volatile__("tlbi alle3is\n\t" : : : "memory");
}

static inline void tlbivaa_el1(uint64_t va)
{
	__asm__ __volatile__("tlbi vaae1, %0\n\t" : : "r" (va) : "memory");
}

struct timestamp_entry;

#define	REG(addr)				((void *)addr)
#define	GCC_GPLL0_USER_CTL			0x0010000C
#define SRC_GPLL0_EVEN_300MHZ			6

enum clk_ctl_gpll_user_ctl {
	CLK_CTL_GPLL_PLLOUT_EVEN_BMSK	= 0x2,
	CLK_CTL_GPLL_PLLOUT_EVEN_SHFT	= 1
};

struct clock_config {
	uint32_t hz;
	uint32_t hw_ctl;
	uint32_t src;
	uint32_t div;
	uint32_t m;
	uint32_t n;
	uint32_t d_2;
};

struct sdm845_clock {
	uint32_t cmd_rcgr;
	uint32_t cfg_rcgr;
	uint32_t m;
	uint32_t n;
	uint32_t d_2;
};

enum clk_ctl_cfg_rcgr {
	CLK_CTL_CFG_HW_CTL_BMSK		= 0x100000,
	CLK_CTL_CFG_HW_CTL_SHFT		= 20,
	CLK_CTL_CFG_MODE_BMSK		= 0x3000,
	CLK_CTL_CFG_MODE_SHFT		= 12,
	CLK_CTL_CFG_SRC_SEL_BMSK	= 0x700,
	CLK_CTL_CFG_SRC_SEL_SHFT	= 8,
	CLK_CTL_CFG_SRC_DIV_BMSK	= 0x1F,
	CLK_CTL_CFG_SRC_DIV_SHFT	= 0
};

enum clk_ctl_cmd_rcgr {
	CLK_CTL_CMD_ROOT_OFF_BMSK	= 0x80000000,
	CLK_CTL_CMD_ROOT_OFF_SHFT	= 31,
	CLK_CTL_CMD_ROOT_EN_BMSK	= 0x2,
	CLK_CTL_CMD_ROOT_EN_SHFT	= 1,
	CLK_CTL_CMD_UPDATE_BMSK		= 0x1,
	CLK_CTL_CMD_UPDATE_SHFT		= 0
};

enum clk_ctl_rcg_mnd {
	CLK_CTL_RCG_MND_BMSK		= 0xFFFF,
	CLK_CTL_RCG_MND_SHFT		= 0,
};

static int clock_configure_mnd(struct sdm845_clock *clk, uint32_t m, uint32_t n,
				uint32_t d_2)
{
	uint32_t reg_val;

	/* Configure Root Clock Generator(RCG) for Dual Edge Mode */
	reg_val = readl(&clk->cfg_rcgr);
	reg_val |= (2 << CLK_CTL_CFG_MODE_SHFT);
	writel(reg_val, &clk->cfg_rcgr);

	/* Set M/N/D config */
	writel(m & CLK_CTL_RCG_MND_BMSK, &clk->m);
	writel(~(n-m) & CLK_CTL_RCG_MND_BMSK, &clk->n);
	writel(~(d_2) & CLK_CTL_RCG_MND_BMSK, &clk->d_2);

	return 0;
}

static int clock_configure(struct sdm845_clock *clk,
				struct clock_config *clk_cfg,
				uint32_t hz, uint32_t num_perfs)
{
	uint32_t reg_val;
	uint32_t idx;

	for (idx = 0; idx < num_perfs; idx++)
		if (hz <= clk_cfg[idx].hz)
			break;

	reg_val = (clk_cfg[idx].src << CLK_CTL_CFG_SRC_SEL_SHFT) |
			(clk_cfg[idx].div << CLK_CTL_CFG_SRC_DIV_SHFT);

	/* Set clock config */
	writel(reg_val, &clk->cfg_rcgr);

	if (clk_cfg[idx].m != 0)
		clock_configure_mnd(clk, clk_cfg[idx].m, clk_cfg[idx].n,
				clk_cfg[idx].d_2);

	/* Commit config to RCG*/
	setbits_le32(&clk->cmd_rcgr, BIT(CLK_CTL_CMD_UPDATE_SHFT));

	return 0;
}

#define	GCC_QUPV3_WRAP1_S1_BASE			0x00118148

static struct sdm845_clock *const qupv3_wrap1_s1_clk =
				(void *)GCC_QUPV3_WRAP1_S1_BASE;

#define DIV(div) (div ? (2*div - 1) : 0)

struct clock_config uart_cfg[] = {
	{
		.hz = 7372800,
		.hw_ctl = 0x0,
		.src = SRC_GPLL0_EVEN_300MHZ,
		.div = DIV(0),
		.m = 384,
		.n = 15625,
		.d_2 = 15625,
	}
};

void clock_init(void)
{
	clock_configure(qupv3_wrap1_s1_clk, uart_cfg, 7372800,
			ARRAY_SIZE(uart_cfg));
}

void bootblock_soc_early_init(void)
{
	clock_init();
}

#define KHz 1000
#define MHz (1000 * 1000)

typedef struct {
	u32 addr;
} gpio_t;

#define QSPI_BASE			0x88DF000
#define TLMM_EAST_TILE_BASE             0x03500000
#define TLMM_NORTH_TILE_BASE            0x03900000
#define TLMM_SOUTH_TILE_BASE            0x03D00000

#define TLMM_TILE_SIZE		0x00400000
#define TLMM_GPIO_OFF_DELTA	0x00001000
#define TLMM_GPIO_TILE_NUM	3

#define TLMM_GPIO_IN_OUT_OFF	0x4
#define TLMM_GPIO_ID_STATUS_OFF	0x10

#define GPIO_FUNC_ENABLE	1
#define GPIO_FUNC_DISABLE	0

/* GPIO TLMM: Direction */
#define GPIO_INPUT	0
#define GPIO_OUTPUT	1

/* GPIO TLMM: Pullup/Pulldown */
#define GPIO_NO_PULL	0
#define GPIO_PULL_DOWN	1
#define GPIO_KEEPER	2
#define GPIO_PULL_UP	3

/* GPIO TLMM: Drive Strength */
#define GPIO_2MA	0
#define GPIO_4MA	1
#define GPIO_6MA	2
#define GPIO_8MA	3
#define GPIO_10MA	4
#define GPIO_12MA	5
#define GPIO_14MA	6
#define GPIO_16MA	7

/* GPIO TLMM: Status */
#define GPIO_DISABLE	0
#define GPIO_ENABLE	1

/* GPIO TLMM: Mask */
#define GPIO_CFG_PULL_BMSK	0x3
#define GPIO_CFG_FUNC_BMSK	0xF
#define GPIO_CFG_DRV_BMSK	0x7
#define GPIO_CFG_OE_BMSK	0x1

/* GPIO TLMM: Shift */
#define GPIO_CFG_PULL_SHFT	0
#define GPIO_CFG_FUNC_SHFT	2
#define GPIO_CFG_DRV_SHFT	6
#define GPIO_CFG_OE_SHFT	9

/* GPIO IO: Mask */
#define GPIO_IO_IN_BMSK		0x1
#define GPIO_IO_OUT_BMSK	0x1

/* GPIO IO: Shift */
#define GPIO_IO_IN_SHFT		0
#define GPIO_IO_OUT_SHFT	1

/* GPIO ID STATUS: Mask */
#define GPIO_ID_STATUS_BMSK	0x1

/* GPIO MAX Valid # */
#define GPIO_NUM_MAX		149

#define GPIO_FUNC_GPIO 0

#define GPIO(num) ((gpio_t){.addr = GPIO##num##_ADDR})

#define PIN(index, tlmm, func1, func2, func3, func4, func5, func6, func7) \
GPIO##index##_ADDR = TLMM_##tlmm##_TILE_BASE + index * TLMM_GPIO_OFF_DELTA, \
GPIO##index##_FUNC_##func1 = 1,	\
GPIO##index##_FUNC_##func2 = 2, \
GPIO##index##_FUNC_##func3 = 3, \
GPIO##index##_FUNC_##func4 = 4, \
GPIO##index##_FUNC_##func5 = 5, \
GPIO##index##_FUNC_##func6 = 6, \
GPIO##index##_FUNC_##func7 = 7

enum {
	PIN(0, EAST,	QUP_L0, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(1, EAST,	QUP_L1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(2, EAST,	QUP_L2, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(3, EAST,	QUP_L3, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(4, NORTH,	QUP_L2, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(5, NORTH,	QUP_L3, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(6, NORTH,	QUP_L0, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(7, NORTH,	QUP_L1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(8, EAST,	QUP_L4_0_CS, GP_PDM_MIRB, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(9, EAST,	QUP_L5_0_CS, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(10, EAST,	MDP_VSYNC_P_MIRA, QUP_L6_0_CS, RES_3, RES_4,
		RES_5, RES_6, RES_7),
	PIN(11, EAST,	MDP_VSYNC_S_MIRA, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(12, SOUTH,	MDP_VSYNC_E, RES_2, TSIF1_SYNC, RES_4, RES_5,
		RES_6, RES_7),
	PIN(13, SOUTH,	RES_1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(14, SOUTH,	RES_1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(15, SOUTH,	RES_1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(16, SOUTH,	RES_1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(17, SOUTH,	CCI_I2C_SDA0, QUP_L0, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(18, SOUTH,	CCI_I2C_SCL0, QUP_L1, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(19, SOUTH,	CCI_I2C_SDA1, QUP_L2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(20, SOUTH,	CCI_I2C_SCL1, QUP_L3, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(21, SOUTH,	CCI_TIMER0, GCC_GP2_CLK_MIRB, RES_3, RES_4,
		RES_5, RES_6, RES_7),
	PIN(22, SOUTH,	CCI_TIMER1, GCC_GP3_CLK_MIRB, RES_3, RES_4,
		RES_5, RES_6, RES_7),
	PIN(23, SOUTH,	CCI_TIMER2, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(24, SOUTH,	CCI_TIMER3, CCI_ASYNC_IN1, RES_3, RES_4,
		RES_5, RES_6, RES_7),
	PIN(25, SOUTH,	CCI_TIMER4, CCI_ASYNC_IN2, RES_3, RES_4,
		RES_5, RES_6, RES_7),
	PIN(26, SOUTH,	CCI_ASYNC_IN0, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(27, EAST,	QUP_L0, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(28, EAST,	QUP_L1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(29, EAST,	QUP_L2, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(30, EAST,	QUP_L3, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(31, NORTH,	QUP_L0, QUP_L2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(32, NORTH,	QUP_L1, QUP_L3, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(33, NORTH,	QUP_L2, QUP_L0, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(34, NORTH,	QUP_L3, QUP_L1, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(35, SOUTH,	PCI_E0_RST_N, QUP_L4_1_CS, RES_3, RES_4,
		RES_5, RES_6, RES_7),
	PIN(36, SOUTH,	PCI_E0_CLKREQN, QUP_L5_1_CS, RES_3, RES_4,
		RES_5, RES_6, RES_7),
	PIN(37, SOUTH,	QUP_L6_1_CS, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(38, NORTH,	USB_PHY_PS, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(39, EAST,	LPASS_SLIMBUS_DATA2, RES_2, RES_3, RES_4,
		RES_5, RES_6, RES_7),
	PIN(40, SOUTH,	SD_WRITE_PROTECT, TSIF1_ERROR, RES_3, RES_4,
		RES_5, RES_6, RES_7),
	PIN(41, EAST,	QUP_L0, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(42, EAST,	QUP_L1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(43, EAST,	QUP_L2, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(44, EAST,	QUP_L3, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(45, EAST,	QUP_L0, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(46, EAST,	QUP_L1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(47, EAST,	QUP_L2, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(48, EAST,	QUP_L3, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(49, NORTH,	QUP_L0, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(50, NORTH,	QUP_L1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(51, NORTH,	QUP_L2, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(52, NORTH,	QUP_L3, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(53, NORTH,	QUP_L2, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(54, NORTH,	QUP_L3, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(55, NORTH,	QUP_L0, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(56, NORTH,	QUP_L1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(57, NORTH,	QUA_MI2S_MCLK, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(58, NORTH,	QUA_MI2S_SCK, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(59, NORTH,	QUA_MI2S_WS, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(60, NORTH,	QUA_MI2S_DATA0, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(61, NORTH,	QUA_MI2S_DATA1, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(62, NORTH,	QUA_MI2S_DATA2, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(63, NORTH,	QUA_MI2S_DATA3, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(64, NORTH,	PRI_MI2S_MCLK, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(65, NORTH,	PRI_MI2S_SCK, QUP_L0, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(66, NORTH,	PRI_MI2S_WS, QUP_L1, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(67, NORTH,	PRI_MI2S_DATA0, QUP_L2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(68, NORTH,	PRI_MI2S_DATA1, QUP_L3, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(69, EAST,	SPKR_I2S_MCLK, AUDIO_REF_CLK, RES_3, RES_4,
		RES_5, RES_6, RES_7),
	PIN(70, EAST,	LPASS_SLIMBUS_CLK, SPKR_I2S_SCK, RES_3, RES_4,
		RES_5, RES_6, RES_7),
	PIN(71, EAST,	LPASS_SLIMBUS_DATA0, SPKR_I2S_DATA_OUT, RES_3,
		RES_4, RES_5, RES_6, RES_7),
	PIN(72, EAST,	LPASS_SLIMBUS_DATA1, SPKR_I2S_WS, RES_3, RES_4,
		RES_5, RES_6, RES_7),
	PIN(73, EAST,	BTFM_SLIMBUS_DATA, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(74, EAST,	BTFM_SLIMBUS_CLK, TER_MI2S_MCLK, RES_3, RES_4,
		RES_5, RES_6, RES_7),
	PIN(75, EAST,	TER_MI2S_SCK, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(76, EAST,	TER_MI2S_WS, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(77, EAST,	TER_MI2S_DATA0, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(78, EAST,	TER_MI2S_DATA1, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(79, NORTH,	SEC_MI2S_MCLK, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(80, NORTH,	SEC_MI2S_SCK, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(81, NORTH,	SEC_MI2S_WS, QUP_L0, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(82, NORTH,	SEC_MI2S_DATA0, QUP_L1, RES_3, RES_4,
		RES_5, RES_6, RES_7),
	PIN(83, NORTH,	SEC_MI2S_DATA1, QUP_L2, RES_3, RES_4,
		RES_5, RES_6, RES_7),
	PIN(84, NORTH,	QUP_L3, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(85, EAST,	QUP_L0, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(86, EAST,	QUP_L1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(87, EAST,	QUP_L2, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(88, EAST,	QUP_L3, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(89, SOUTH,	TSIF1_CLK, QUP_L0, QSPI_CS_N_1, RES_4, RES_5,
		RES_6, RES_7),
	PIN(90, SOUTH,	TSIF1_EN, MDP_VSYNC0_OUT, QUP_L1, QSPI_CS_N_0,
		MDP_VSYNC1_OUT, MDP_VSYNC2_OUT, MDP_VSYNC3_OUT),
	PIN(91, SOUTH,	TSIF1_DATA, SDC4_CMD, QUP_L2, QSPI_DATA,
		RES_5, RES_6, RES_7),
	PIN(92, SOUTH,	TSIF2_ERROR, SDC4_DATA, QUP_L3, QSPI_DATA,
		RES_5, RES_6, RES_7),
	PIN(93, SOUTH,	TSIF2_CLK, SDC4_CLK, QUP_L0, QSPI_DATA,
		RES_5, RES_6, RES_7),
	PIN(94, SOUTH,	TSIF2_EN, SDC4_DATA, QUP_L1, QSPI_DATA,
		RES_5, RES_6, RES_7),
	PIN(95, SOUTH,	TSIF2_DATA, SDC4_DATA, QUP_L2, QSPI_CLK,
		RES_5, RES_6, RES_7),
	PIN(96, SOUTH,	TSIF2_SYNC, SDC4_DATA, QUP_L3, RES_4,
		RES_5, RES_6, RES_7),
	PIN(97, NORTH,	RFFE6_CLK, GRFC37, MDP_VSYNC_P_MIRB,
		RES_4, RES_5, RES_6, RES_7),
	PIN(98, NORTH,	RFFE6_DATA, MDP_VSYNC_S_MIRB, RES_3,
		RES_4, RES_5, RES_6, RES_7),
	PIN(99, NORTH,	RES_1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(100, NORTH,	RES_1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(101, NORTH,	GRFC4, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(102, NORTH,	PCI_E1_RST_N, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(103, NORTH,	PCI_E1_CLKREQN, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(104, NORTH,	RES_1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(105, NORTH,	UIM2_DATA, QUP_L0, QUP_L4_8_CS, RES_4, RES_5,
		RES_6, RES_7),
	PIN(106, NORTH,	UIM2_CLK, QUP_L1, QUP_L5_8_CS, RES_4, RES_5,
		RES_6, RES_7),
	PIN(107, NORTH,	UIM2_RESET, QUP_L2, QUP_L6_8_CS, RES_4, RES_5,
		RES_6, RES_7),
	PIN(108, NORTH,	UIM2_PRESENT, QUP_L3, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(109, NORTH,	UIM1_DATA, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(110, NORTH,	UIM1_CLK, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(111, NORTH,	UIM1_RESET, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(112, NORTH,	UIM1_PRESENT, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(113, NORTH,	UIM_BATT_ALARM, EDP_HOT_PLUG_DETECT, RES_3,
		RES_4, RES_5, RES_6, RES_7),
	PIN(114, NORTH,	GRFC8, RES_2, RES_3, GPS_TX_AGGRESSOR_MIRE,
		RES_5, RES_6, RES_7),
	PIN(115, NORTH,	GRFC9, RES_2, RES_3, GPS_TX_AGGRESSOR_MIRF,
		RES_5, RES_6, RES_7),
	PIN(116, NORTH,	RES_1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(117, NORTH,	RES_1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(118, NORTH,	RES_1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(119, NORTH,	RES_1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(120, NORTH,	RES_1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(121, NORTH,	RES_1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(122, EAST,	RES_1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(123, EAST,	QUP_L4_9_CS, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(124, EAST,	QUP_L5_9_CS, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(125, EAST,	QUP_L6_9_CS, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(126, EAST,	RES_1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(127, NORTH,	GRFC3, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(128, NORTH,	RES_1, RES_2, GPS_TX_AGGRESSOR_MIRB, RES_4,
		RES_5, RES_6, RES_7),
	PIN(129, NORTH,	RES_1, RES_2, GPS_TX_AGGRESSOR_MIRC, RES_4,
		RES_5, RES_6, RES_7),
	PIN(130, NORTH,	QLINK_REQUEST, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(131, NORTH,	QLINK_ENABLE, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(132, NORTH,	GRFC2, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(133, NORTH,	RES_1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(134, NORTH,	RES_1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(135, NORTH,	GRFC0, PA_INDICATOR_1_OR_2, RES_3, RES_4,
		RES_5, RES_6, RES_7),
	PIN(136, NORTH,	GRFC1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(137, NORTH,	RFFE3_DATA, GRFC35, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(138, NORTH,	RFFE3_CLK, GRFC32, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(139, NORTH,	RFFE4_DATA, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(140, NORTH,	RFFE4_CLK, GRFC36, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(141, NORTH,	RFFE5_DATA, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(142, NORTH,	RFFE5_CLK, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(143, NORTH,	GRFC5, RES_2, RES_3, GPS_TX_AGGRESSOR_MIRD,
		RES_5, RES_6, RES_7),
	PIN(144, NORTH,	RES_1, RES_2, RES_3, RES_4, RES_5, RES_6, RES_7),
	PIN(145, NORTH,	RES_1, GPS_TX_AGGRESSOR_MIRA, RES_3, RES_4,
		RES_5, RES_6, RES_7),
	PIN(146, NORTH,	RFFE2_DATA, GRFC34, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(147, NORTH,	RFFE2_CLK, GRFC33, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(148, NORTH,	RFFE1_DATA, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
	PIN(149, NORTH,	RFFE1_CLK, RES_2, RES_3, RES_4, RES_5,
		RES_6, RES_7),
};

struct tlmm_gpio {
	uint32_t cfg;
	uint32_t in_out;
};

void gpio_configure(gpio_t gpio, uint32_t func, uint32_t pull,
				uint32_t drive_str, uint32_t enable)
{
	uint32_t reg_val;
	struct tlmm_gpio *regs = (void *)(uintptr_t)gpio.addr;

	reg_val = ((enable & GPIO_CFG_OE_BMSK) << GPIO_CFG_OE_SHFT) |
		  ((drive_str & GPIO_CFG_DRV_BMSK) << GPIO_CFG_DRV_SHFT) |
		  ((func & GPIO_CFG_FUNC_BMSK) << GPIO_CFG_FUNC_SHFT) |
		  ((pull & GPIO_CFG_PULL_BMSK) << GPIO_CFG_PULL_SHFT);

	writel(reg_val, &regs->cfg);
}

void gpio_set(gpio_t gpio, int value)
{
	struct tlmm_gpio *regs = (void *)(uintptr_t)gpio.addr;
	writel((!!value) << GPIO_IO_OUT_SHFT, &regs->in_out);
}

int gpio_get(gpio_t gpio)
{
	struct tlmm_gpio *regs = (void *)(uintptr_t)gpio.addr;

	return ((readl(&regs->in_out) >> GPIO_IO_IN_SHFT) &
		GPIO_IO_IN_BMSK);
}

void gpio_input_pulldown(gpio_t gpio)
{
	gpio_configure(gpio, GPIO_FUNC_DISABLE,
				GPIO_PULL_DOWN, GPIO_2MA, GPIO_DISABLE);
}

void gpio_input_pullup(gpio_t gpio)
{
	gpio_configure(gpio, GPIO_FUNC_DISABLE,
				GPIO_PULL_UP, GPIO_2MA, GPIO_DISABLE);
}

void gpio_input(gpio_t gpio)
{
	gpio_configure(gpio, GPIO_FUNC_DISABLE,
				GPIO_NO_PULL, GPIO_2MA, GPIO_DISABLE);
}

void gpio_output(gpio_t gpio, int value)
{
	gpio_set(gpio, value);
	gpio_configure(gpio, GPIO_FUNC_DISABLE,
				GPIO_NO_PULL, GPIO_2MA, GPIO_ENABLE);
}

/* Calculate divisor. Do not floor but round to nearest integer. */
unsigned int uart_baudrate_divisor(unsigned int baudrate,
	unsigned int refclk, unsigned int oversample)
{
	return (1 + (2 * refclk) / (baudrate * oversample)) / 2;
}

struct mono_time {
	long microseconds;
};

struct stopwatch {
	struct mono_time start;
	struct mono_time current;
	struct mono_time expires;
};

/* Set an absolute time to a number of microseconds. */
static inline void mono_time_set_usecs(struct mono_time *mt, long us)
{
	mt->microseconds = us;
}

void timer_monotonic_get(struct mono_time *mt)
{
	uint64_t tvalue = raw_read_cntpct_el0();
	uint32_t tfreq  = raw_read_cntfrq_el0();
	long usecs = (tvalue * 1000000) / tfreq;
	mono_time_set_usecs(mt, usecs);
}

static inline void stopwatch_init(struct stopwatch *sw)
{
	if (1) //IS_ENABLED(CONFIG_HAVE_MONOTONIC_TIMER))
		timer_monotonic_get(&sw->start);
	else
		sw->start.microseconds = 0;

	sw->current = sw->expires = sw->start;
}

/* Compare two absolute times: Return -1, 0, or 1 if t1 is <, =, or > t2,
 * respectively. */
static inline int mono_time_cmp(const struct mono_time *t1,
				const struct mono_time *t2)
{
	if (t1->microseconds == t2->microseconds)
		return 0;

	if (t1->microseconds < t2->microseconds)
		return -1;

	return 1;
}

/*
 * Tick the stopwatch to collect the current time.
 */
static inline void stopwatch_tick(struct stopwatch *sw)
{
	if (1) //IS_ENABLED(CONFIG_HAVE_MONOTONIC_TIMER))
		timer_monotonic_get(&sw->current);
	else
		sw->current.microseconds = 0;
}

/* Return time difference between t1 and t2. i.e. t2 - t1. */
static inline long mono_time_diff_microseconds(const struct mono_time *t1,
					       const struct mono_time *t2)
{
	return t2->microseconds - t1->microseconds;
}

/*
 * Return number of microseconds since starting the stopwatch.
 */
static inline long stopwatch_duration_usecs(struct stopwatch *sw)
{
	/*
	 * If the stopwatch hasn't been ticked (current == start) tick
	 * the stopwatch to gather the accumulated time.
	 */
	if (!mono_time_cmp(&sw->start, &sw->current))
		stopwatch_tick(sw);

	return mono_time_diff_microseconds(&sw->start, &sw->current);
}

/* Helper function to allow bitbanging an 8n1 UART. */
void uart_bitbang_tx_byte(unsigned char data, void (*set_tx)(int line_state))
{
	const int baud_rate = 115200; // get_uart_baudrate();
	int i;
	struct stopwatch sw;
	stopwatch_init(&sw);

	/* Send start bit */
	set_tx(0);
	while (stopwatch_duration_usecs(&sw) < MHz / baud_rate)
		stopwatch_tick(&sw);

	/* 'i' counts the total bits sent at the end of the loop */
	for (i = 2; i < 10; i++) {
		set_tx(data & 1);
		data >>= 1;
		while (stopwatch_duration_usecs(&sw) < i * MHz / baud_rate)
			stopwatch_tick(&sw);
	}

	/* Send stop bit */
	set_tx(1);
	while (stopwatch_duration_usecs(&sw) < i * MHz / baud_rate)
		stopwatch_tick(&sw);
}

#define UART_TX_PIN GPIO(4)

static void set_tx(int line_state)
{
	gpio_set(UART_TX_PIN, line_state);
}

void uart_tx_byte(int idx, unsigned char data)
{
	uart_bitbang_tx_byte(data, set_tx);
}

void uart_init(int idx)
{
	gpio_output(UART_TX_PIN, 1);
	uart_tx_byte(0, 'b');
	while (1);
}

void bootblock_main_with_timestamp(uint64_t base_timestamp,
	struct timestamp_entry *timestamps, size_t num_timestamps)
{
#if 0
	/* Initialize timestamps if we have TIMESTAMP region in memlayout.ld. */
	if (IS_ENABLED(CONFIG_COLLECT_TIMESTAMPS) && _timestamp_size > 0) {
		int i;
		timestamp_init(base_timestamp);
		for (i = 0; i < num_timestamps; i++)
			timestamp_add(timestamps[i].entry_id,
				      timestamps[i].entry_stamp);
	}

	sanitize_cmos();
	cmos_post_init();
#endif
	bootblock_soc_early_init();
// 	bootblock_mainboard_early_init();
	uart_init(0);
}

void init_timer(void)
{
	raw_write_cntfrq_el0(19200*KHz);
}

void cb_main(void)
{
	uint64_t base_timestamp = 0;

	init_timer();

// 	if (IS_ENABLED(CONFIG_COLLECT_TIMESTAMPS))
// 		base_timestamp = timestamp_get();

	bootblock_main_with_timestamp(base_timestamp, NULL, 0);
}

u32 spl_boot_device(void)
{
	return 0;
}

void hang(void)
{
	while (1);
}

int timer_init(void)
{
	return 0;
}

void *memset(void *ptr,int ch, __kernel_size_t count)
{
	char *s8;

	s8 = (char *)ptr;
	while (count--)
		*s8++ = ch;

	return ptr;
}

void * memcpy(void *dest, const void *src, size_t count)
{
	unsigned long *dl = (unsigned long *)dest, *sl = (unsigned long *)src;
	char *d8, *s8;

	if (src == dest)
		return dest;

	/* while all data is aligned (common case), copy a word at a time */
	if ( (((ulong)dest | (ulong)src) & (sizeof(*dl) - 1)) == 0) {
		while (count >= sizeof(*dl)) {
			*dl++ = *sl++;
			count -= sizeof(*dl);
		}
	}
	/* copy the reset one byte at a time */
	d8 = (char *)dl;
	s8 = (char *)sl;
	while (count--)
		*d8++ = *s8++;

	return dest;
}
