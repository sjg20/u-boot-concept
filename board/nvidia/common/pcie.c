/*
 * arch/arm/mach-tegra/pci.c
 *
 * PCIe host controller driver for TEGRA(2) SOCs
 *
 * Copyright (c) 2010, CompuLab, Ltd.
 * Author: Mike Rapoport <mike@compulab.co.il>
 *
 * Based on NVIDIA PCIe driver
 * Copyright (c) 2008-2009, NVIDIA Corporation.
 *
 * Bits taken from arch/arm/mach-dove/pcie.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <common.h>
#include <pci.h>

#include <asm/errno.h>
#include <asm/sizes.h>

#include "compat.h"

/* register definitions */
#define AFI_OFFSET	0x3800
#define PADS_OFFSET	0x3000
#define RP0_OFFSET	0x0000
#define RP1_OFFSET	0x1000

#define AFI_AXI_BAR0_SZ	0x00
#define AFI_AXI_BAR1_SZ	0x04
#define AFI_AXI_BAR2_SZ	0x08
#define AFI_AXI_BAR3_SZ	0x0c
#define AFI_AXI_BAR4_SZ	0x10
#define AFI_AXI_BAR5_SZ	0x14

#define AFI_AXI_BAR0_START	0x18
#define AFI_AXI_BAR1_START	0x1c
#define AFI_AXI_BAR2_START	0x20
#define AFI_AXI_BAR3_START	0x24
#define AFI_AXI_BAR4_START	0x28
#define AFI_AXI_BAR5_START	0x2c

#define AFI_FPCI_BAR0	0x30
#define AFI_FPCI_BAR1	0x34
#define AFI_FPCI_BAR2	0x38
#define AFI_FPCI_BAR3	0x3c
#define AFI_FPCI_BAR4	0x40
#define AFI_FPCI_BAR5	0x44

#define AFI_CACHE_BAR0_SZ	0x48
#define AFI_CACHE_BAR0_ST	0x4c
#define AFI_CACHE_BAR1_SZ	0x50
#define AFI_CACHE_BAR1_ST	0x54

#define AFI_MSI_BAR_SZ		0x60
#define AFI_MSI_FPCI_BAR_ST	0x64
#define AFI_MSI_AXI_BAR_ST	0x68

#define AFI_CONFIGURATION		0xac
#define  AFI_CONFIGURATION_EN_FPCI	(1 << 0)

#define AFI_FPCI_ERROR_MASKS	0xb0

#define AFI_INTR_MASK		0xb4
#define  AFI_INTR_MASK_INT_MASK	(1 << 0)
#define  AFI_INTR_MASK_MSI_MASK	(1 << 8)

#define AFI_INTR_CODE		0xb8
#define  AFI_INTR_CODE_MASK	0xf
#define  AFI_INTR_MASTER_ABORT	4
#define  AFI_INTR_LEGACY	6

#define AFI_INTR_SIGNATURE	0xbc
#define AFI_SM_INTR_ENABLE	0xc4

#define AFI_AFI_INTR_ENABLE		0xc8
#define  AFI_INTR_EN_INI_SLVERR		(1 << 0)
#define  AFI_INTR_EN_INI_DECERR		(1 << 1)
#define  AFI_INTR_EN_TGT_SLVERR		(1 << 2)
#define  AFI_INTR_EN_TGT_DECERR		(1 << 3)
#define  AFI_INTR_EN_TGT_WRERR		(1 << 4)
#define  AFI_INTR_EN_DFPCI_DECERR	(1 << 5)
#define  AFI_INTR_EN_AXI_DECERR		(1 << 6)
#define  AFI_INTR_EN_FPCI_TIMEOUT	(1 << 7)

#define AFI_PCIE_CONFIG					0x0f8
#define  AFI_PCIE_CONFIG_PCIEC0_DISABLE_DEVICE		(1 << 1)
#define  AFI_PCIE_CONFIG_PCIEC1_DISABLE_DEVICE		(1 << 2)
#define  AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_MASK	(0xf << 20)
#define  AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_SINGLE	(0x0 << 20)
#define  AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_DUAL	(0x1 << 20)

#define AFI_FUSE			0x104
#define  AFI_FUSE_PCIE_T0_GEN2_DIS	(1 << 2)

#define AFI_PEX0_CTRL			0x110
#define AFI_PEX1_CTRL			0x118
#define  AFI_PEX_CTRL_RST		(1 << 0)
#define  AFI_PEX_CTRL_REFCLK_EN		(1 << 3)

#define RP_VEND_XP	0x00000F00
#define  RP_VEND_XP_DL_UP	(1 << 30)

#define RP_LINK_CONTROL_STATUS			0x00000090
#define  RP_LINK_CONTROL_STATUS_LINKSTAT_MASK	0x3fff0000

#define PADS_CTL_SEL		0x0000009C

#define PADS_CTL		0x000000A0
#define  PADS_CTL_IDDQ_1L	(1 << 0)
#define  PADS_CTL_TX_DATA_EN_1L	(1 << 6)
#define  PADS_CTL_RX_DATA_EN_1L	(1 << 10)

#define PADS_PLL_CTL				0x000000B8
#define  PADS_PLL_CTL_RST_B4SM			(1 << 1)
#define  PADS_PLL_CTL_LOCKDET			(1 << 8)
#define  PADS_PLL_CTL_REFCLK_MASK		(0x3 << 16)
#define  PADS_PLL_CTL_REFCLK_INTERNAL_CML	(0 << 16)
#define  PADS_PLL_CTL_REFCLK_INTERNAL_CMOS	(1 << 16)
#define  PADS_PLL_CTL_REFCLK_EXTERNAL		(2 << 16)
#define  PADS_PLL_CTL_TXCLKREF_MASK		(0x1 << 20)
#define  PADS_PLL_CTL_TXCLKREF_DIV10		(0 << 20)
#define  PADS_PLL_CTL_TXCLKREF_DIV5		(1 << 20)

/* PMC access is required for PCIE xclk (un)clamping */
#define PMC_SCRATCH42		0x144
#define PMC_SCRATCH42_PCX_CLAMP	(1 << 0)

static volatile void *reg_pmc_base = (volatile void *)TEGRA_PMC_BASE;
static volatile void *reg_clk_base = (volatile void *)TEGRA_CLK_RESET_BASE;

#define pmc_writel(value, reg) \
	writel(value, (u32)reg_pmc_base + (reg))
#define pmc_readl(reg) \
	readl((u32)reg_pmc_base + (reg))
#define clk_writel(value, reg) \
	writel(value, (u32)reg_clk_base + (reg))
#define clk_readl(reg) \
	readl((u32)reg_clk_base + (reg))

/*
 * Clocks and powergating
 */
#define RST_DEVICES_SET			0x300
#define RST_DEVICES_CLR			0x304

#define CLK_OUT_ENB_SET			0x320
#define CLK_OUT_ENB_CLR			0x324

#define PLLE_REG			0xe8
#define PLL_BASE_BYPASS			(1 << 31)
#define PLL_BASE_ENABLE			(1 << 30)
#define PLLE_MISC_READY			(1 << 15)

#define PERIPH_CLK_TO_ENB_REG(clk)	((clk / 32) * 4)
#define PERIPH_CLK_TO_ENB_SET_REG(clk)	((clk / 32) * 8)
#define PERIPH_CLK_TO_ENB_BIT(clk)	(1 << (clk % 32))

#define TEGRA_PCIE_PEX_CLK	70
#define TEGRA_PCIE_AFI_CLK	72
#define TEGRA_PCIE_PCIE_XCLK	74

#define TEGRA_POWERGATE_PCIE	3
#define TEGRA_POWERGATE_VDEC	4

#define PWRGATE_TOGGLE		0x30
#define  PWRGATE_TOGGLE_START	(1 << 8)
#define REMOVE_CLAMPING		0x34
#define PWRGATE_STATUS		0x38

static char * clk_name(int clk_num)
{
	switch (clk_num) {
	case TEGRA_PCIE_PEX_CLK: return "pex";
	case TEGRA_PCIE_AFI_CLK: return "afi";
	case TEGRA_PCIE_PCIE_XCLK: return "xclk";
	default: return "unknown";
	}
}

static void tegra2_periph_clk_reset(int clk_num, int assert)
{
	unsigned long base = assert ? RST_DEVICES_SET : RST_DEVICES_CLR;

	debug("%s %s on clock %s\n", __func__,
	       assert ? "assert" : "deassert", clk_name(clk_num));
	clk_writel(PERIPH_CLK_TO_ENB_BIT(clk_num),
		   base + PERIPH_CLK_TO_ENB_SET_REG(clk_num));
}

static void tegra_periph_reset_assert(int clk_num)
{
	tegra2_periph_clk_reset(clk_num, 1);
}

static void tegra_periph_reset_deassert(int clk_num)
{
	tegra2_periph_clk_reset(clk_num, 0);
}

static int tegra_pcie_clk_enable(int clk_num)
{
	debug("%s on clock %s\n", __func__, clk_name(clk_num));

	clk_writel(PERIPH_CLK_TO_ENB_BIT(clk_num),
		   CLK_OUT_ENB_SET + PERIPH_CLK_TO_ENB_SET_REG(clk_num));
	return 0;
}

static void tegra_pcie_clk_disable(int clk_num)
{
	debug("%s on clock %s\n", __func__, clk_name(clk_num));

	clk_writel(PERIPH_CLK_TO_ENB_BIT(clk_num),
		   CLK_OUT_ENB_CLR + PERIPH_CLK_TO_ENB_SET_REG(clk_num));
}

static int tegra_pcie_plle_enable(void)
{
	u32 val;

	debug("%s on clock %s\n", __func__, "pll e");

	mdelay(1);

	val = clk_readl(PLLE_REG);
	if (!(val & PLLE_MISC_READY))
		return -EBUSY;

	val = clk_readl(PLLE_REG);
	val |= PLL_BASE_ENABLE | PLL_BASE_BYPASS;
	clk_writel(val, PLLE_REG);

	return 0;
}

static int tegra_powergate_set(int id, int new_state)
{
	int status;

	status = pmc_readl(PWRGATE_STATUS) & (1 << id);

	if (status == new_state)
		return -EINVAL;

	pmc_writel(PWRGATE_TOGGLE_START | id, PWRGATE_TOGGLE);

	return 0;
}

static int tegra_powergate_power_on(int id)
{
	return tegra_powergate_set(id, 1);
}

static int tegra_powergate_power_off(int id)
{
	return tegra_powergate_set(id, 0);
}

int tegra_powergate_remove_clamping(int id)
{
	u32 mask;

	/*
	 * Tegra 2 has a bug where PCIE and VDE clamping masks are
	 * swapped relatively to the partition ids
	 */
	if (id ==  TEGRA_POWERGATE_VDEC)
		mask = (1 << TEGRA_POWERGATE_PCIE);
	else if	(id == TEGRA_POWERGATE_PCIE)
		mask = (1 << TEGRA_POWERGATE_VDEC);
	else
		mask = (1 << id);

	pmc_writel(mask, REMOVE_CLAMPING);

	return 0;
}

static int tegra_powergate_sequence_power_up(int id, int clk)
{
	int ret;

	tegra_periph_reset_assert(clk);

	ret = tegra_powergate_power_on(id);
	if (ret)
		goto err_power;

	ret = tegra_pcie_clk_enable(clk);
	if (ret)
		goto err_clk;

	udelay(10);

	ret = tegra_powergate_remove_clamping(id);
	if (ret)
		goto err_clamp;

	udelay(10);
	tegra_periph_reset_deassert(clk);

	return 0;

err_clamp:
	tegra_pcie_clk_disable(clk);
err_clk:
	tegra_powergate_power_off(id);
err_power:
	return ret;
}

/*
 * Tegra2 defines 1GB in the AXI address map for PCIe.
 *
 * That address space is split into different regions, with sizes and
 * offsets as follows:
 *
 * 0x80000000 - 0x80003fff - PCI controller registers
 * 0x80004000 - 0x80103fff - PCI configuration space
 * 0x80104000 - 0x80203fff - PCI extended configuration space
 * 0x80203fff - 0x803fffff - unused
 * 0x80400000 - 0x8040ffff - downstream IO
 * 0x80410000 - 0x8fffffff - unused
 * 0x90000000 - 0x9fffffff - non-prefetchable memory
 * 0xa0000000 - 0xbfffffff - prefetchable memory
 */
#define TEGRA_PCIE_BASE		0x80000000

#define PCIE_REGS_SZ		SZ_16K
#define PCIE_CFG_OFF		PCIE_REGS_SZ
#define PCIE_CFG_SZ		SZ_1M
#define PCIE_EXT_CFG_OFF	(PCIE_CFG_SZ + PCIE_CFG_OFF)
#define PCIE_EXT_CFG_SZ		SZ_1M
#define PCIE_IOMAP_SZ		(PCIE_REGS_SZ + PCIE_CFG_SZ + PCIE_EXT_CFG_SZ)

#define MMIO_BASE		(TEGRA_PCIE_BASE + SZ_4M)
#define MMIO_SIZE		SZ_64K
#define MEM_BASE_0		(TEGRA_PCIE_BASE + SZ_256M)
#define MEM_SIZE_0		SZ_128M
#define MEM_BASE_1		(MEM_BASE_0 + MEM_SIZE_0)
#define MEM_SIZE_1		SZ_128M
#define PREFETCH_MEM_BASE_0	(MEM_BASE_1 + MEM_SIZE_1)
#define PREFETCH_MEM_SIZE_0	SZ_128M
#define PREFETCH_MEM_BASE_1	(PREFETCH_MEM_BASE_0 + PREFETCH_MEM_SIZE_0)
#define PREFETCH_MEM_SIZE_1	SZ_128M

#define  PCIE_CONF_BUS(b)	((b) << 16)
#define  PCIE_CONF_DEV(d)	((d) << 11)
#define  PCIE_CONF_FUNC(f)	((f) << 8)
#define  PCIE_CONF_REG(r)	\
	(((r) & ~0x3) | (((r) < 256) ? PCIE_CFG_OFF : PCIE_EXT_CFG_OFF))

struct tegra_pcie_port {
	int			index;
	u8			root_bus_nr;
	volatile void		*base;

	int			link_up;
};

struct tegra_pcie_info {
	struct tegra_pcie_port	port[2];
	int			num_ports;

	volatile void		*regs;
};

static struct tegra_pcie_info tegra_pcie = {
};

static inline void afi_writel(u32 value, unsigned long offset)
{
	writel(value, offset + AFI_OFFSET + tegra_pcie.regs);
}

static inline u32 afi_readl(unsigned long offset)
{
	return readl(offset + AFI_OFFSET + tegra_pcie.regs);
}

static inline void pads_writel(u32 value, unsigned long offset)
{
	writel(value, offset + PADS_OFFSET + tegra_pcie.regs);
}

static inline u32 pads_readl(unsigned long offset)
{
	return readl(offset + PADS_OFFSET + tegra_pcie.regs);
}

static struct tegra_pcie_port *bus_to_port(int bus)
{
	int i;

	for (i = tegra_pcie.num_ports - 1; i >= 0; i--) {
		int rbus = tegra_pcie.port[i].root_bus_nr;
		if (rbus != -1 && rbus == bus)
			break;
	}

	return i >= 0 ? tegra_pcie.port + i : NULL;
}

static int tegra_pcie_read_conf_dword(struct pci_controller *hose,
				      pci_dev_t dev, int where, u32 *val)
{
	int bus = PCI_BUS (dev);
	struct tegra_pcie_port *pp = bus_to_port(bus);
	volatile void *addr;

	/* FIXME: currently read_conf hangs if PCI_DEV > 0 */
	if (PCI_DEV(dev) != 0) {
		*val = 0xffffffff;
		return 1;
	}

	if (pp) {
		if (where == PCI_CLASS_REVISION) {
			*val &= ~0xff;
			*val |= (PCI_CLASS_BRIDGE_PCI << 16);
			return 0;
		}

		addr = pp->base + (where & ~0x3);
	} else {
		addr = tegra_pcie.regs + (PCIE_CONF_BUS(bus) +
					  PCIE_CONF_DEV(PCI_DEV(dev)) +
					  PCIE_CONF_FUNC(PCI_FUNC(dev)) +
					  PCIE_CONF_REG(where));
	}

	*val = readl(addr);

	return 0;
}

static int tegra_pcie_write_conf_dword(struct pci_controller* hose,
				 pci_dev_t dev, int where, u32 val)
{
	int bus = PCI_BUS (dev);
	struct tegra_pcie_port *pp = bus_to_port(bus);
	volatile void *addr;

	/* FIXME: currently read_conf hangs if PCI_DEV > 0 */
	if (PCI_DEV(dev) != 0)
		return 1;

	if (pp)
		addr = pp->base + (where & ~0x3);
	else
		addr = tegra_pcie.regs + (PCIE_CONF_BUS(bus) +
					  PCIE_CONF_DEV(PCI_DEV(dev)) +
					  PCIE_CONF_FUNC(PCI_FUNC(dev)) +
					  PCIE_CONF_REG(where));

	writel(val, addr);
	return 0;
}

#if 0
/* FIXME: enable setting of relaxed ordering */
/* Tegra PCIE requires relaxed ordering */
static void tegra_pcie_relax_enable(struct pci_dev *dev)
{
	u16 val16;
	int pos = pci_find_capability(dev, PCI_CAP_ID_EXP);

	if (pos <= 0) {
		dev_err(&dev->dev, "skipping relaxed ordering fixup\n");
		return;
	}

	pci_read_config_word(dev, pos + PCI_EXP_DEVCTL, &val16);
	val16 |= PCI_EXP_DEVCTL_RELAX_EN;
	pci_write_config_word(dev, pos + PCI_EXP_DEVCTL, val16);
}

/* FIXME: refactor port number assignment and host controller registration */
static struct pci_bus *tegra_pcie_scan_bus(int nr,
					   struct pci_sys_data *sys)
{
	struct tegra_pcie_port *pp;

	if (nr >= tegra_pcie.num_ports)
		return 0;

	pp = tegra_pcie.port + nr;
	pp->root_bus_nr = sys->busnr;

	return pci_scan_bus(sys->busnr, &tegra_pcie_ops, sys);
}
#endif

static void tegra_pcie_setup_translations(void)
{
	u32 fpci_bar;
	u32 size;
	u32 axi_address;

	/* Bar 0: config Bar */
	fpci_bar = ((u32)0xfdff << 16);
	size = PCIE_CFG_SZ;
	axi_address = TEGRA_PCIE_BASE + PCIE_CFG_OFF;
	afi_writel(axi_address, AFI_AXI_BAR0_START);
	afi_writel(size >> 12, AFI_AXI_BAR0_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR0);

	/* Bar 1: extended config Bar */
	fpci_bar = ((u32)0xfe1 << 20);
	size = PCIE_EXT_CFG_SZ;
	axi_address = TEGRA_PCIE_BASE + PCIE_EXT_CFG_OFF;
	afi_writel(axi_address, AFI_AXI_BAR1_START);
	afi_writel(size >> 12, AFI_AXI_BAR1_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR1);

	/* Bar 2: downstream IO bar */
	fpci_bar = ((__u32)0xfdfc << 16);
	size = MMIO_SIZE;
	axi_address = MMIO_BASE;
	afi_writel(axi_address, AFI_AXI_BAR2_START);
	afi_writel(size >> 12, AFI_AXI_BAR2_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR2);

	/* Bar 3: prefetchable memory BAR */
	fpci_bar = (((PREFETCH_MEM_BASE_0 >> 12) & 0x0fffffff) << 4) | 0x1;
	size =  PREFETCH_MEM_SIZE_0 +  PREFETCH_MEM_SIZE_1;
	axi_address = PREFETCH_MEM_BASE_0;
	afi_writel(axi_address, AFI_AXI_BAR3_START);
	afi_writel(size >> 12, AFI_AXI_BAR3_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR3);

	/* Bar 4: non prefetchable memory BAR */
	fpci_bar = (((MEM_BASE_0 >> 12)	& 0x0FFFFFFF) << 4) | 0x1;
	size = MEM_SIZE_0 + MEM_SIZE_1;
	axi_address = MEM_BASE_0;
	afi_writel(axi_address, AFI_AXI_BAR4_START);
	afi_writel(size >> 12, AFI_AXI_BAR4_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR4);

	/* Bar 5: NULL out the remaining BAR as it is not used */
	fpci_bar = 0;
	size = 0;
	axi_address = 0;
	afi_writel(axi_address, AFI_AXI_BAR5_START);
	afi_writel(size >> 12, AFI_AXI_BAR5_SZ);
	afi_writel(fpci_bar, AFI_FPCI_BAR5);

	/* map all upstream transactions as uncached */
	afi_writel(PHYS_SDRAM_1, AFI_CACHE_BAR0_ST);
	afi_writel(0, AFI_CACHE_BAR0_SZ);
	afi_writel(0, AFI_CACHE_BAR1_ST);
	afi_writel(0, AFI_CACHE_BAR1_SZ);

	/* No MSI */
	afi_writel(0, AFI_MSI_FPCI_BAR_ST);
	afi_writel(0, AFI_MSI_BAR_SZ);
	afi_writel(0, AFI_MSI_AXI_BAR_ST);
	afi_writel(0, AFI_MSI_BAR_SZ);
}

static void tegra_pcie_enable_controller(void)
{
	u32 val, reg;
	int i;

	/* Enable slot clock and pulse the reset signals */
	for (i = 0, reg = AFI_PEX0_CTRL; i < 2; i++, reg += 0x8) {
		val = afi_readl(reg) |  AFI_PEX_CTRL_REFCLK_EN;
		afi_writel(val, reg);
		val &= ~AFI_PEX_CTRL_RST;
		afi_writel(val, reg);

		val = afi_readl(reg) | AFI_PEX_CTRL_RST;
		afi_writel(val, reg);
	}

	/* Enable dual controller and both ports */
	val = afi_readl(AFI_PCIE_CONFIG);
	val &= ~(AFI_PCIE_CONFIG_PCIEC0_DISABLE_DEVICE |
		 AFI_PCIE_CONFIG_PCIEC1_DISABLE_DEVICE |
		 AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_MASK);
	val |= AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_DUAL;
	afi_writel(val, AFI_PCIE_CONFIG);

	val = afi_readl(AFI_FUSE) & ~AFI_FUSE_PCIE_T0_GEN2_DIS;
	afi_writel(val, AFI_FUSE);

	/* Initialze internal PHY, enable up to 16 PCIE lanes */
	pads_writel(0x0, PADS_CTL_SEL);

	/* override IDDQ to 1 on all 4 lanes */
	val = pads_readl(PADS_CTL) | PADS_CTL_IDDQ_1L;
	pads_writel(val, PADS_CTL);

	/*
	 * set up PHY PLL inputs select PLLE output as refclock,
	 * set TX ref sel to div10 (not div5)
	 */
	val = pads_readl(PADS_PLL_CTL);
	val &= ~(PADS_PLL_CTL_REFCLK_MASK | PADS_PLL_CTL_TXCLKREF_MASK);
	val |= (PADS_PLL_CTL_REFCLK_INTERNAL_CML | PADS_PLL_CTL_TXCLKREF_DIV10);
	pads_writel(val, PADS_PLL_CTL);

	/* take PLL out of reset  */
	val = pads_readl(PADS_PLL_CTL) | PADS_PLL_CTL_RST_B4SM;
	pads_writel(val, PADS_PLL_CTL);

	/*
	 * Hack, set the clock voltage to the DEFAULT provided by hw folks.
	 * This doesn't exist in the documentation
	 */
	pads_writel(0xfa5cfa5c, 0xc8);

	/* Wait for the PLL to lock */
	do {
		val = pads_readl(PADS_PLL_CTL);
	} while (!(val & PADS_PLL_CTL_LOCKDET));

	/* turn off IDDQ override */
	val = pads_readl(PADS_CTL) & ~PADS_CTL_IDDQ_1L;
	pads_writel(val, PADS_CTL);

	/* enable TX/RX data */
	val = pads_readl(PADS_CTL);
	val |= (PADS_CTL_TX_DATA_EN_1L | PADS_CTL_RX_DATA_EN_1L);
	pads_writel(val, PADS_CTL);

	/* Take the PCIe interface module out of reset */
	tegra_periph_reset_deassert(TEGRA_PCIE_PCIE_XCLK);


	/* Finally enable PCIe */
	val = afi_readl(AFI_CONFIGURATION) | AFI_CONFIGURATION_EN_FPCI;
	afi_writel(val, AFI_CONFIGURATION);

	val = (AFI_INTR_EN_INI_SLVERR | AFI_INTR_EN_INI_DECERR |
	       AFI_INTR_EN_TGT_SLVERR | AFI_INTR_EN_TGT_DECERR |
	       AFI_INTR_EN_TGT_WRERR | AFI_INTR_EN_DFPCI_DECERR);
	afi_writel(val, AFI_AFI_INTR_ENABLE);
	afi_writel(0xffffffff, AFI_SM_INTR_ENABLE);

	/* FIXME: No MSI for now, only INT */
	afi_writel(AFI_INTR_MASK_INT_MASK, AFI_INTR_MASK);

	/* Disable all execptions */
	afi_writel(0, AFI_FPCI_ERROR_MASKS);

	return;
}

static void tegra_pcie_xclk_clamp(int clamp)
{
	u32 reg;

	reg = pmc_readl(PMC_SCRATCH42) & ~PMC_SCRATCH42_PCX_CLAMP;

	if (clamp)
		reg |= PMC_SCRATCH42_PCX_CLAMP;

	pmc_writel(reg, PMC_SCRATCH42);
}

static int tegra_pcie_power_on(void)
{
	int err;

	tegra_pcie_xclk_clamp(1);
	tegra_periph_reset_assert(TEGRA_PCIE_PEX_CLK);

	tegra_periph_reset_assert(TEGRA_PCIE_AFI_CLK);
	err = tegra_powergate_sequence_power_up(TEGRA_POWERGATE_PCIE,
						TEGRA_PCIE_PEX_CLK);
	if (err) {
		printf("PCIE: powerup sequence failed: %d\n", err);
		return err;
	}
	tegra_periph_reset_deassert(TEGRA_PCIE_AFI_CLK);

	tegra_pcie_xclk_clamp(0);

	tegra_pcie_clk_enable(TEGRA_PCIE_AFI_CLK);
	tegra_pcie_clk_enable(TEGRA_PCIE_PEX_CLK);

	return tegra_pcie_plle_enable();
	return 0;
}

static void tegra_pcie_power_off(void)
{
	tegra_periph_reset_assert(TEGRA_PCIE_PCIE_XCLK);
	tegra_periph_reset_assert(TEGRA_PCIE_AFI_CLK);
	tegra_periph_reset_assert(TEGRA_PCIE_PEX_CLK);

	tegra_powergate_power_off(TEGRA_POWERGATE_PCIE);
	tegra_pcie_xclk_clamp(1);
}

static int tegra_pcie_get_resources(void)
{
	int err;

	tegra_pcie_power_off();

	err = tegra_pcie_power_on();
	if (err) {
		printf("PCIE: failed to power up: %d\n", err);
		return err;
	}

	tegra_pcie.regs = (volatile void*)TEGRA_PCIE_BASE;

	return 0;
}

/*
 * FIXME: If there are no PCIe cards attached, then calling this function
 * can result in the increase of the bootup time as there are big timeout
 * loops.
 */
#define TEGRA_PCIE_LINKUP_TIMEOUT	200	/* up to 1.2 seconds */
static int tegra_pcie_check_link(struct tegra_pcie_port *pp, int idx,
				  u32 reset_reg)
{
	u32 reg;
	int retries = 3;
	int timeout;

	do {
		timeout = TEGRA_PCIE_LINKUP_TIMEOUT;
		while (timeout) {
			reg = readl(pp->base + RP_VEND_XP);

			if (reg & RP_VEND_XP_DL_UP)
				break;

			mdelay(1);
			timeout--;
		}

		if (!timeout)  {
			printf("PCIE: port %d: link down, retrying\n", idx);
			goto retry;
		}

		timeout = TEGRA_PCIE_LINKUP_TIMEOUT;
		while (timeout) {
			reg = readl(pp->base + RP_LINK_CONTROL_STATUS);

			if (reg & 0x20000000)
				return 1;

			mdelay(1);
			timeout--;
		}

retry:
		/* Pulse the PEX reset */
		reg = afi_readl(reset_reg) | AFI_PEX_CTRL_RST;
		afi_writel(reg, reset_reg);
		mdelay(1);
		reg = afi_readl(reset_reg) & ~AFI_PEX_CTRL_RST;
		afi_writel(reg, reset_reg);

		retries--;
	} while (retries);

	return 0;
}

static void tegra_pcie_add_port(int index, u32 offset, u32 reset_reg)
{
	struct tegra_pcie_port *pp;

	pp = tegra_pcie.port + tegra_pcie.num_ports;

	pp->index = -1;
	pp->base = tegra_pcie.regs + offset;
	pp->link_up = tegra_pcie_check_link(pp, index, reset_reg);

	if (!pp->link_up) {
		pp->base = NULL;
		printf("PCIE: port %d: link down, ignoring\n", index);
		return;
	}

	tegra_pcie.num_ports++;
	pp->index = index;
	pp->root_bus_nr = -1;
}

struct pci_controller pcie_hose = {
	.first_busno	= 0,
	.last_busno	= 0xff,
};

static void tegra_pcie_register_hose(void)
{
	struct tegra_pcie_port *pp;

	/* PCI memory space */
	pci_set_region(pcie_hose.regions + 0,
		       MEM_BASE_0, MEM_BASE_0,
		       MEM_SIZE_0 + MEM_SIZE_1,
		       PCI_REGION_MEM);

	/* PCI I/O space */
	pci_set_region (pcie_hose.regions + 1,
			MMIO_BASE, MMIO_BASE,
			MMIO_SIZE, PCI_REGION_IO);

	/* PCI prefetch memory */
	pci_set_region (pcie_hose.regions + 2,
			PREFETCH_MEM_BASE_0, PREFETCH_MEM_BASE_0,
			PREFETCH_MEM_SIZE_0 + PREFETCH_MEM_SIZE_1,
			PCI_REGION_MEM | PCI_REGION_PREFETCH);

	/* System memory */
	pci_set_region (pcie_hose.regions + 3,
			PHYS_SDRAM_1, PHYS_SDRAM_1,
			SZ_1M << 10,
			PCI_REGION_MEM | PCI_REGION_SYS_MEMORY);

	pcie_hose.region_count = 4;

	pci_set_ops(&pcie_hose,
		    pci_hose_read_config_byte_via_dword,
		    pci_hose_read_config_word_via_dword,
		    tegra_pcie_read_conf_dword,
		    pci_hose_write_config_byte_via_dword,
		    pci_hose_write_config_word_via_dword,
		    tegra_pcie_write_conf_dword);

	pci_register_hose(&pcie_hose);

	pp = tegra_pcie.port + 0;
	pp->root_bus_nr = 0;

	pcie_hose.last_busno = pci_hose_scan(&pcie_hose);
}

int tegra_pcie_init(int init_port0, int init_port1)
{
	int err;

	if (!(init_port0 || init_port1))
		return -ENODEV;

	err = tegra_pcie_get_resources();
	if (err)
		return err;

	tegra_pcie_enable_controller();

	/* setup the AFI address translations */
	tegra_pcie_setup_translations();

	if (init_port0)
		tegra_pcie_add_port(0, RP0_OFFSET, AFI_PEX0_CTRL);

	if (init_port1)
		tegra_pcie_add_port(1, RP1_OFFSET, AFI_PEX1_CTRL);

	tegra_pcie_register_hose();

	return 0;
}

int pci_skip_dev(struct pci_controller *hose, pci_dev_t dev)
{
	return 0;
}
