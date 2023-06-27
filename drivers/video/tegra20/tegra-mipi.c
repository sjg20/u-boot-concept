// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2013 NVIDIA Corporation
 * Copyright (c) 2023 Svyatoslav Ryhel <clamor95@gmail.com>
 */

#include <common.h>
#include <dm.h>
#include <misc.h>
#include <linux/delay.h>

#include <asm/io.h>
#include <asm/arch/clock.h>

/* MIPI control registers 0x00 ~ 0x60 */
struct mipi_ctlr {
	uint mipi_cal_ctrl;
	uint mipi_cal_autocal_ctrl;
	uint mipi_cal_status;

	uint unused1[2];

	uint mipi_cal_config_csia;
	uint mipi_cal_config_csib;
	uint mipi_cal_config_csic;
	uint mipi_cal_config_csid;
	uint mipi_cal_config_csie;
	uint mipi_cal_config_csif;

	uint unused2[3];

	uint mipi_cal_config_dsia;
	uint mipi_cal_config_dsib;
	uint mipi_cal_config_dsic;
	uint mipi_cal_config_dsid;

	uint unused3[4];

	uint mipi_cal_bias_pad_cfg0;
	uint mipi_cal_bias_pad_cfg1;
	uint mipi_cal_bias_pad_cfg2;
};

#define MIPI_CAL_CTRL_NOISE_FILTER(x)	(((x) & 0xf) << 26)
#define MIPI_CAL_CTRL_PRESCALE(x)	(((x) & 0x3) << 24)
#define MIPI_CAL_CTRL_CLKEN_OVR		BIT(4)
#define MIPI_CAL_CTRL_START		BIT(0)

#define MIPI_CAL_STATUS_DONE		BIT(16)
#define MIPI_CAL_STATUS_ACTIVE		BIT(0)

/* for data and clock lanes */
#define MIPI_CAL_CONFIG_SELECT		BIT(21)

/* for data lanes */
#define MIPI_CAL_CONFIG_HSPDOS(x)	(((x) & 0x1f) << 16)
#define MIPI_CAL_CONFIG_HSPUOS(x)	(((x) & 0x1f) <<  8)
#define MIPI_CAL_CONFIG_TERMOS(x)	(((x) & 0x1f) <<  0)

#define MIPI_CAL_BIAS_PAD_PDVCLAMP	BIT(1)
#define MIPI_CAL_BIAS_PAD_E_VCLAMP_REF	BIT(0)

#define MIPI_CAL_BIAS_PAD_DRV_DN_REF(x) (((x) & 0x7) << 16)
#define MIPI_CAL_BIAS_PAD_DRV_UP_REF(x) (((x) & 0x7) << 8)

#define MIPI_CAL_BIAS_PAD_VCLAMP(x)	(((x) & 0x7) << 16)
#define MIPI_CAL_BIAS_PAD_VAUXP(x)	(((x) & 0x7) << 4)
#define MIPI_CAL_BIAS_PAD_PDVREG	BIT(1)

struct tegra_mipi_soc {
	bool clock_enable_override;
	bool needs_vclamp_ref;

	/* bias pad configuration settings */
	u8 pad_drive_down_ref;
	u8 pad_drive_up_ref;

	u8 pad_vclamp_level;
	u8 pad_vauxp_level;

	/* calibration settings for data lanes */
	u8 hspdos;
	u8 hspuos;
	u8 termos;

	/* calibration settings for clock lanes */
	u8 hsclkpdos;
	u8 hsclkpuos;
};

struct tegra_mipi_priv {
	struct mipi_ctlr *mipi;
	const struct tegra_mipi_soc *soc;
	int mipi_clk;
};

static int tegra_mipi_calibrate(struct udevice *dev, int offset, const void *buf,
				int size)
{
	struct tegra_mipi_priv *priv = dev_get_priv(dev);
	const struct tegra_mipi_soc *soc = priv->soc;
	u32 value;
//	int ret;

	value = MIPI_CAL_BIAS_PAD_DRV_DN_REF(soc->pad_drive_down_ref) |
		MIPI_CAL_BIAS_PAD_DRV_UP_REF(soc->pad_drive_up_ref);
	writel(value, &priv->mipi->mipi_cal_bias_pad_cfg1);

	value = readl(&priv->mipi->mipi_cal_bias_pad_cfg2);
	value &= ~MIPI_CAL_BIAS_PAD_VCLAMP(0x7);
	value &= ~MIPI_CAL_BIAS_PAD_VAUXP(0x7);
	value |= MIPI_CAL_BIAS_PAD_VCLAMP(soc->pad_vclamp_level);
	value |= MIPI_CAL_BIAS_PAD_VAUXP(soc->pad_vauxp_level);
	writel(value, &priv->mipi->mipi_cal_bias_pad_cfg2);

	/* T114 SOC configuration */
	value = MIPI_CAL_CONFIG_SELECT |
		MIPI_CAL_CONFIG_HSPDOS(soc->hspdos) |
		MIPI_CAL_CONFIG_HSPUOS(soc->hspuos) |
		MIPI_CAL_CONFIG_TERMOS(soc->termos);
	writel(value, &priv->mipi->mipi_cal_config_csia);
	writel(value, &priv->mipi->mipi_cal_config_csib);
	writel(value, &priv->mipi->mipi_cal_config_csic);
	writel(value, &priv->mipi->mipi_cal_config_csid);
	writel(value, &priv->mipi->mipi_cal_config_csie);
	writel(value, &priv->mipi->mipi_cal_config_dsia);
	writel(value, &priv->mipi->mipi_cal_config_dsib);
	writel(value, &priv->mipi->mipi_cal_config_dsic);
	writel(value, &priv->mipi->mipi_cal_config_dsid);

	value = readl(&priv->mipi->mipi_cal_ctrl);
	value &= ~MIPI_CAL_CTRL_NOISE_FILTER(0xf);
	value &= ~MIPI_CAL_CTRL_PRESCALE(0x3);
	value |= MIPI_CAL_CTRL_NOISE_FILTER(0xa);
	value |= MIPI_CAL_CTRL_PRESCALE(0x2);

	if (!soc->clock_enable_override)
		value &= ~MIPI_CAL_CTRL_CLKEN_OVR;
	else
		value |= MIPI_CAL_CTRL_CLKEN_OVR;

	writel(value, &priv->mipi->mipi_cal_ctrl);

	/* clear any pending status bits */
	value = readl(&priv->mipi->mipi_cal_status);
	writel(value, &priv->mipi->mipi_cal_status);

	value = readl(&priv->mipi->mipi_cal_ctrl);
	value |= MIPI_CAL_CTRL_START;
	writel(value, &priv->mipi->mipi_cal_ctrl);

	/*
	 * Wait for min 72uS to let calibration logic finish calibration
	 * sequence codes before waiting for pads idle state to apply the
	 * results.
	 */
	udelay(80);

//	ret = readl_relaxed_poll_timeout(status_reg, value,
//					 !(value & MIPI_CAL_STATUS_ACTIVE) &&
//					 (value & MIPI_CAL_STATUS_DONE), 50,
//					 250000);

	return 0;
}

static int tegra_mipi_enable(struct udevice *dev, bool val)
{
	struct tegra_mipi_priv *priv = dev_get_priv(dev);
	u32 value;

	clock_enable(priv->mipi_clk);
	udelay(2);
	reset_set_enable(priv->mipi_clk, 0);

	value = readl(&priv->mipi->mipi_cal_bias_pad_cfg0);
	value &= ~MIPI_CAL_BIAS_PAD_PDVCLAMP;

	if (priv->soc->needs_vclamp_ref)
		value |= MIPI_CAL_BIAS_PAD_E_VCLAMP_REF;

	writel(value, &priv->mipi->mipi_cal_bias_pad_cfg0);

	value = readl(&priv->mipi->mipi_cal_bias_pad_cfg2);
	value &= ~MIPI_CAL_BIAS_PAD_PDVREG;
	writel(value, &priv->mipi->mipi_cal_bias_pad_cfg2);

	return 0;
}

static const struct misc_ops tegra_mipi_ops = {
	.write = tegra_mipi_calibrate,
	.set_enabled = tegra_mipi_enable,
};

static const struct tegra_mipi_soc tegra114_mipi_soc = {
	.clock_enable_override = true,
	.needs_vclamp_ref = true,
	.pad_drive_down_ref = 0x2,
	.pad_drive_up_ref = 0x0,
	.pad_vclamp_level = 0x0,
	.pad_vauxp_level = 0x0,
	.hspdos = 0x0,
	.hspuos = 0x4,
	.termos = 0x5,
	.hsclkpdos = 0x0,
	.hsclkpuos = 0x4,
};

static int tegra_mipi_probe(struct udevice *dev)
{
	struct tegra_mipi_priv *priv = dev_get_priv(dev);

	priv->mipi = (struct mipi_ctlr *)dev_read_addr_ptr(dev);
	if (!priv->mipi) {
		log_err("no MIPI controller address\n");
		return -EINVAL;
	}

	priv->mipi_clk = clock_decode_periph_id(dev);

	priv->soc = &tegra114_mipi_soc;

	return 0;
}

static const struct udevice_id tegra_mipi_ids[] = {
	{ .compatible = "nvidia,tegra114-mipi" },
	{ }
};

U_BOOT_DRIVER(tegra_mipi) = {
	.name           = "tegra_mipi",
	.id             = UCLASS_MISC,
	.ops		= &tegra_mipi_ops,
	.of_match       = tegra_mipi_ids,
	.probe          = tegra_mipi_probe,
	.priv_auto	= sizeof(struct tegra_mipi_priv),
};
