// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <asm/arch/iomap.h>
#include <asm/arch/fsp/fsp_configs.h>
#include <asm/arch/fsp/fsp_m_upd.h>
#include <asm/fsp2/fsp_internal.h>
#include <dm/uclass-internal.h>

/*
 * LPDDR4 helper routines for configuring the memory UPD for LPDDR4 operation.
 * There are four physical LPDDR4 channels, each 32-bits wide. There are two
 * logical channels using two physical channels together to form a 64-bit
 * interface to memory for each logical channel.
 */

enum {
	LP4_PHYS_CH0A,
	LP4_PHYS_CH0B,
	LP4_PHYS_CH1A,
	LP4_PHYS_CH1B,

	LP4_NUM_PHYS_CHANNELS,
};

/*
 * The DQs within a physical channel can be bit-swizzled within each byte.
 * Within a channel the bytes can be swapped, but the DQs need to be routed
 * with the corresponding DQS (strobe).
 */
enum {
	LP4_DQS0,
	LP4_DQS1,
	LP4_DQS2,
	LP4_DQS3,

	LP4_NUM_BYTE_LANES,
	DQ_BITS_PER_DQS		= 8,
};

/* Provide bit swizzling per DQS and byte swapping within a channel */
struct lpddr4_chan_swizzle_cfg {
	u8 dqs[LP4_NUM_BYTE_LANES][DQ_BITS_PER_DQS];
};

struct lpddr4_swizzle_cfg {
	struct lpddr4_chan_swizzle_cfg phys[LP4_NUM_PHYS_CHANNELS];
};

int fspm_update_config(struct udevice *dev, struct fspm_upd *upd)
{
	struct fsp_m_config *cfg = &upd->config;
	struct fspm_arch_upd *arch = &upd->arch;
	char chx_buf[30];
	const struct lpddr4_chan_swizzle_cfg *sch;
	const struct lpddr4_swizzle_cfg *swizzle_cfg;
	const size_t sz = DQ_BITS_PER_DQS;
	bool tmp;
	const u8 *gpio_table_pins;
	const char *oem_file;

	arch->nvs_buffer_ptr = NULL;
	prepare_mrc_cache(upd);
	arch->stack_base = (void *)0xfef96000;
	arch->boot_loader_tolum_size = 0;
	arch->boot_mode = FSP_BOOT_WITH_FULL_CONFIGURATION;

	cfg->serial_debug_port_type = dev_read_u32_default(dev,
						"fspm,serial-debug-port-type",
						SERIAL_DEBUG_PORT_TYPE_MMIO);
	cfg->serial_debug_port_device = dev_read_u32_default(dev,
						"fspm,serial-debug-port-device",
						SERIAL_DEBUG_PORT_DEVICE_UART2);
	if (cfg->serial_debug_port_device == SERIAL_DEBUG_PORT_DEVICE_EXTERNAL)
		cfg->serial_debug_port_address = dev_read_u32_default(dev,
				"fspm,serial-debug-port-address",
				0);
	cfg->serial_debug_port_stride_size = dev_read_u32_default(dev,
					"fspm,serial-debug-port-stride-size",
					SERIAL_DEBUG_PORT_STRIDE_SIZE_4);
	cfg->mrc_fast_boot = !dev_read_bool(dev, "fspm,disable-mrc-fast-boot");
	cfg->igd = !dev_read_bool(dev, "fspm,disable-igd");
	if (cfg->igd) {
		cfg->igd_dvmt50_pre_alloc = dev_read_u32_default(dev,
						"fspm,igd-dvmt50-pre-alloc",
						IGD_DVMT_50_PRE_ALLOC_64M);
		cfg->igd_aperture_size = dev_read_u32_default(dev,
						"fspm,aperture-size",
						IGD_APERTURE_SIZE_128M);
		cfg->gtt_size = dev_read_u32_default(dev, "fspm,gtt-size",
						     GTT_SIZE_8M);
		cfg->primary_video_adaptor = dev_read_u32_default(dev,
						"fspm,primary-video-adaptor",
						PRIMARY_VIDEO_ADAPTER_AUTO);
	}
	cfg->package = dev_read_u32_default(dev, "fspm,package",
					    PACKAGE_SODIMM);
	cfg->profile = dev_read_u32_default(dev, "fspm,profile",
					    PROFILE_DDR3_1600_11_11_11);
	cfg->memory_down = dev_read_u32_default(dev, "fspm,memory-down",
						MEMORY_DOWN_NO);
	if (cfg->memory_down & MEMORY_DOWN_YES) {
		cfg->ddr3_l_page_size = dev_read_u32_default(dev,
						"fspm,ddr3l-page-size", 1);
		cfg->ddr3_lasr = dev_read_bool(dev, "fspm,enable-ddr3-lasr");
	}
	cfg->scrambler_support = dev_read_bool(dev,
					       "fspm,enable-scrambler-support");
	tmp = dev_read_bool(dev, "enable-interleaved-mode");
	cfg->interleaved_mode = (tmp == 0) ? INTERLEAVED_MODE_DISABLE :
					     INTERLEAVED_MODE_ENABLE;
	cfg->channel_hash_mask = dev_read_u32_default(dev,
						      "fspm,channel-hash-mask",
						      0);
	cfg->slice_hash_mask = dev_read_u32_default(dev,
						    "fspm,slice-hash-mask", 0);
	cfg->channels_slices_enable = dev_read_bool(dev,
						"fspm,enable-channels-slices");
	cfg->min_ref_rate2x_enable = dev_read_bool(dev,
						"fspm,enable-min-ref-rate2x");
	cfg->dual_rank_support_enable = !dev_read_bool(dev,
					"fspm,disable-dual-rank-support");
	tmp = dev_read_bool(dev, "fspm,enable-rmt-mode");
	cfg->rmt_mode = (tmp == 0) ? RMT_MODE_DISABLE : RMT_MODE_ENABLE;
	cfg->memory_size_limit = dev_read_u32_default(dev,
						"fspm,memory-size-limit", 0);
	cfg->low_memory_max_value = dev_read_u32_default(dev,
						"fspm,low-memory-max-value", 0);
	cfg->high_memory_max_value = dev_read_u32_default(dev,
						"fspm,high-memory-max-value",
						0);
	cfg->disable_fast_boot = dev_read_bool(dev, "fspm,disable-fast-boot");
	cfg->dimm0_spd_address = 0;
	cfg->dimm1_spd_address = 0;
	if (!(cfg->memory_down & MEMORY_DOWN_YES)) {
		cfg->dimm0_spd_address = dev_read_u32_default(dev,
						"fspm,dimm0-spd-address", 0xa0);
		cfg->dimm1_spd_address = dev_read_u32_default(dev,
						"fspm,dimm1-spd-address", 0xa4);
	}
	if (cfg->memory_down != 0) {
		for (int i = 0; i < FSP_DRAM_CHANNELS; i++)  {
			snprintf(chx_buf, sizeof(chx_buf),
				 "fspm,ch%d-enable-rank", i);
			cfg->chan[i].rank_enable =  dev_read_bool(dev, chx_buf);
			snprintf(chx_buf, sizeof(chx_buf),
				 "fspm,ch%d-device-width", i);
			cfg->chan[i].device_width = dev_read_u32_default(dev,
								chx_buf, 0);
			snprintf(chx_buf, sizeof(chx_buf),
				 "fspm,ch%d-dram-density", i);
			cfg->chan[i].dram_density = dev_read_u32_default(dev,
								chx_buf, 0);
			snprintf(chx_buf, sizeof(chx_buf),
				 "fspm,ch%d-option", i);
			cfg->chan[i].option = dev_read_u32_default(dev, chx_buf,
								   0);
			snprintf(chx_buf, sizeof(chx_buf),
				 "fspm,ch%d-odt-config", i);
			cfg->chan[i].odt_config = dev_read_u32_default(dev,
								       chx_buf,
								       0);
			cfg->chan[i].tristate_clk1 = 0;
			snprintf(chx_buf, sizeof(chx_buf),
				 "fspm,ch%d-enable-mode2-n", i);
			cfg->chan[i].mode2_n = dev_read_u32_default(dev,
								    chx_buf, 0);
			snprintf(chx_buf, sizeof(chx_buf),
				 "fspm,ch%d-odt-levels", i);
			cfg->chan[i].odt_levels = dev_read_u32_default(dev,
					chx_buf,
					CHX_ODT_LEVELS_CONNECTED_TO_SOC);
		}
	}
	cfg->rmt_check_run = dev_read_bool(dev,
				"fspm,rmt-degrade-margin-check");
	cfg->rmt_margin_check_scale_high_threshold =  dev_read_u32_default(dev,
				"fspm,rmt-margin-check-scale-high-threshold",
				0);
	swizzle_cfg = (const struct lpddr4_swizzle_cfg *)dev_read_u8_array_ptr(
							dev, "lpddr4-swizzle",
							LP4_NUM_BYTE_LANES *
							DQ_BITS_PER_DQS *
							LP4_NUM_PHYS_CHANNELS);

	if (swizzle_cfg) {
		/*
		 * CH0_DQB byte lanes in the bit swizzle configuration field are
		 * not 1:1. The mapping within the swizzling field is:
		 *   indices [0:7]   - byte lane 1 (DQS1) DQ[8:15]
		 *   indices [8:15]  - byte lane 0 (DQS0) DQ[0:7]
		 *   indices [16:23] - byte lane 3 (DQS3) DQ[24:31]
		 *   indices [24:31] - byte lane 2 (DQS2) DQ[16:23]
		 */
		sch = &swizzle_cfg->phys[LP4_PHYS_CH0B];
		memcpy(&cfg->ch_bit_swizzling[0][0], &sch->dqs[LP4_DQS1], sz);
		memcpy(&cfg->ch_bit_swizzling[0][8], &sch->dqs[LP4_DQS0], sz);
		memcpy(&cfg->ch_bit_swizzling[0][16], &sch->dqs[LP4_DQS3], sz);
		memcpy(&cfg->ch_bit_swizzling[0][24], &sch->dqs[LP4_DQS2], sz);
		/*
		 * CH0_DQA byte lanes in the bit swizzle configuration field are
		 * 1:1.
		 */
		sch = &swizzle_cfg->phys[LP4_PHYS_CH0A];
		memcpy(&cfg->ch_bit_swizzling[1][0], &sch->dqs[LP4_DQS0], sz);
		memcpy(&cfg->ch_bit_swizzling[1][8], &sch->dqs[LP4_DQS1], sz);
		memcpy(&cfg->ch_bit_swizzling[1][16], &sch->dqs[LP4_DQS2], sz);
		memcpy(&cfg->ch_bit_swizzling[1][24], &sch->dqs[LP4_DQS3], sz);
		sch = &swizzle_cfg->phys[LP4_PHYS_CH1B];
		memcpy(&cfg->ch_bit_swizzling[2][0], &sch->dqs[LP4_DQS1], sz);
		memcpy(&cfg->ch_bit_swizzling[2][8], &sch->dqs[LP4_DQS0], sz);
		memcpy(&cfg->ch_bit_swizzling[2][16], &sch->dqs[LP4_DQS3], sz);
		memcpy(&cfg->ch_bit_swizzling[2][24], &sch->dqs[LP4_DQS2], sz);
		/*
		 * CH0_DQA byte lanes in the bit swizzle configuration field are
		 * 1:1.
		 */
		sch = &swizzle_cfg->phys[LP4_PHYS_CH1A];
		memcpy(&cfg->ch_bit_swizzling[3][0], &sch->dqs[LP4_DQS0], sz);
		memcpy(&cfg->ch_bit_swizzling[3][8], &sch->dqs[LP4_DQS1], sz);
		memcpy(&cfg->ch_bit_swizzling[3][16], &sch->dqs[LP4_DQS2], sz);
		memcpy(&cfg->ch_bit_swizzling[3][24], &sch->dqs[LP4_DQS3], sz);
	}
	cfg->msg_level_mask = dev_read_u32_default(dev, "fspm,msg-level-mask",
						   0);
	memset(cfg->pre_mem_gpio_table_pin_num, 0,
	       sizeof(cfg->pre_mem_gpio_table_pin_num));
	gpio_table_pins = dev_read_u8_array_ptr(dev,
					"fspm,pre-mem-gpio-table-pin-num",
					sizeof(gpio_table_pins));
	if (gpio_table_pins)
		memcpy(cfg->pre_mem_gpio_table_pin_num, gpio_table_pins,
		       sizeof(gpio_table_pins));
	cfg->pre_mem_gpio_table_ptr = dev_read_u32_default(dev,
						"fspm,pre-mem-gpio-table-ptr",
						0);
	cfg->pre_mem_gpio_table_entry_num = dev_read_u32_default(dev,
					"fspm,pre-mem-gpio-table-entry-num",
					0);
	cfg->enhance_port8xh_decoding = !dev_read_bool(dev,
				"fspm,disable-enhance-port8xh-decoding");
	cfg->spd_write_enable = dev_read_bool(dev, "fspm,enable-spd-write");
	cfg->oem_loading_base = dev_read_u32_default(dev,
						"fspm,oem-loading-base",
						0);
	memset(cfg->oem_file_name, 0, sizeof(cfg->oem_file_name));
	oem_file = dev_read_string(dev, "oem-file-name");
	if (oem_file)
		memcpy(cfg->oem_file_name, oem_file,
		       sizeof(cfg->oem_file_name));
	cfg->mrc_data_saving = dev_read_bool(dev,
					     "fspm,enable_mrc-data-saving");
	cfg->e_mmc_trace_len = dev_read_bool(dev, "emmc-trace-len-short");
	cfg->skip_cse_rbp = dev_read_bool(dev, "fspm,enable-skip-cse-rbp");
	cfg->npk_en = dev_read_u32_default(dev, "fspm,enable-npk", NPK_EN_AUTO);
	cfg->fw_trace_en = !dev_read_bool(dev, "fspm,disable-fw-trace");
	cfg->fw_trace_destination = dev_read_u32_default(dev,
					"fspm,fw-trace-destination",
					FW_TRACE_DESTINATION_NPK_TRACE_TO_PTI);
	cfg->recover_dump =  dev_read_bool(dev, "fspm,enable-recover-dump");
	cfg->msc0_wrap = dev_read_u32_default(dev, "msc0-wrap", MSC_X_WRAP_1);
	cfg->msc1_wrap = dev_read_u32_default(dev, "msc1-wrap", MSC_X_WRAP_1);
	cfg->msc0_size = dev_read_u32_default(dev, "fspm,msc0-size",
					      MSC_X_SIZE_0M);
	cfg->msc1_size = dev_read_u32_default(dev, "fspm,msc1-size",
					      MSC_X_SIZE_0M);
	cfg->pti_mode = dev_read_u32_default(dev, "fspm,pti-mode", PTI_MODE_x4);
	cfg->pti_training = dev_read_u32_default(dev, "fspm,pti-training", 0);
	cfg->pti_speed = dev_read_u32_default(dev, "fspm,pti-speed",
					      PTI_SPEED_QUARTER);
	cfg->punit_mlvl = dev_read_u32_default(dev, "fspm,punit-mlvl", 0);
	cfg->pmc_mlvl = dev_read_u32_default(dev, "fspm,pmc-mlvl", 0);
	cfg->sw_trace_en = dev_read_bool(dev, "fspm,enable-sw-trace");
	cfg->periodic_retraining_disable = dev_read_bool(dev,
					"fspm,disable-periodic-retraining");
	cfg->enable_reset_system = dev_read_bool(dev, "enable-reset-system");
	cfg->enable_s3_heci2 = !dev_read_bool(dev, "fspm,disable-s3-heci2");

	return 0;
}

/*
 * The FSP-M binary appears to break the SPI controller. It can be fixed by
 * writing the BAR again, so do that here
 */
int fspm_done(struct udevice *dev)
{
	struct udevice *spi;
	int ret;

	/* Don't probe the device, since that reads the BAR */
	ret = uclass_find_first_device(UCLASS_SPI, &spi);
	if (ret)
		return log_msg_ret("SPI", ret);
	if (!spi)
		return log_msg_ret("no SPI", -ENODEV);

	dm_pci_write_config32(spi, PCI_BASE_ADDRESS_0,
			      IOMAP_SPI_BASE | PCI_BASE_ADDRESS_SPACE_MEMORY);

	return 0;
}
