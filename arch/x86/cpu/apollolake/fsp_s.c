// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <binman.h>
#include <dm.h>
#include <irq.h>
#include <malloc.h>
#include <acpi/acpi_s3.h>
#include <asm/intel_pinctrl.h>
#include <asm/io.h>
#include <asm/intel_regs.h>
#include <asm/msr.h>
#include <asm/msr-index.h>
#include <asm/pci.h>
#include <asm/arch/cpu.h>
#include <asm/arch/systemagent.h>
#include <asm/arch/fsp/fsp_configs.h>
#include <asm/arch/fsp/fsp_s_upd.h>

#define PCH_P2SB_E0		0xe0
#define HIDE_BIT		BIT(0)

#define INTEL_GSPI_MAX		3
#define MAX_USB2_PORTS		8

#define FSP_I2C_COUNT 8
#define FSP_HSUART_COUNT 4
#define FSP_SPI_COUNT 3

const u8 pcie_rp_clk_req_number_def[6] = { 0x4, 0x5, 0x0, 0x1, 0x2, 0x3 };
const u8 physical_slot_number_def[6] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5 };
const u8 ipc_def[16] = { 0xf8, 0xef, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
const u8 port_usb20_per_port_pe_txi_set_def[8] = { 0x07, 0x06, 0x06, 0x06, 0x07,
						  0x07, 0x07, 0x01 };
const u8 port_usb20_per_port_txi_set_def[8] = { 0x00, 0x00, 0x00, 0x00, 0x00,
					       0x00, 0x00, 0x03};
const u8 port_usb20_hs_skew_sel_def[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					  0x00, 0x01 };
const u8 port_usb20_i_usb_tx_emphasis_en_def[8] = { 0x03, 0x03, 0x03, 0x03,
						   0x03, 0x03, 0x03, 0x01 };
const u8 port_usb20_hs_npre_drv_sel_def[8] = { 0x00, 0x00, 0x00, 0x00, 0x00,
					      0x00, 0x00, 0x03 };

static int read_u8_array(u8 prop[], ofnode node, const char *propname, int sz,
			  u8 def)
{
	const u8 *ptr;

	ptr = ofnode_read_u8_array_ptr(node, propname, sz);
	if (ptr) {
		memcpy(prop, ptr, sz);
	} else {
		memset(prop, def, sz);
		return -EINVAL;
	}

	return 0;
}

static int read_u16_array(u16 prop[], ofnode node, const char *propname, int sz,
			   u16 def)
{
	u32 *array_buf;
	int ret;

	array_buf = malloc(sz * sizeof(u32));
	if (!array_buf)
		return -ENOMEM;

	ret = ofnode_read_u32_array(node, propname, array_buf, sz);
	if (!ret) {
		for (int i = 0; i < sz; i++)
			prop[i] = array_buf[i];
	} else if (ret == -EINVAL) {
		for (int i = 0; i < sz; i++)
			prop[i] = def;
		ret = 0;
	}
	free(array_buf);

	return ret;
}

static int read_u32_array(u32 prop[], ofnode node, const char *propname, int sz,
			   u32 def)
{
	int ret;

	ret = ofnode_read_u32_array(node, propname, prop, sz);
	if (ret == -EINVAL) {
		for (int i = 0; i < sz; i++)
			prop[i] = def;
		ret = 0;
	}

	return ret;
}

int fsps_update_config(struct udevice *dev, ulong rom_offset,
		       struct fsps_upd *upd)
{
	struct fsp_s_config *cfg = &upd->config;
	int ret;
	char buf[32];
	ofnode node;

#ifdef CONFIG_HAVE_VBT
	struct binman_entry vbt;
	void *vbt_buf;

	ret = binman_entry_find("intel-vbt", &vbt);
	if (ret)
		return log_msg_ret("Cannot find VBT", ret);
	vbt.image_pos += rom_offset;
	vbt_buf = malloc(vbt.size);
	if (!vbt_buf)
		return log_msg_ret("Alloc VBT", -ENOMEM);

	/*
	 * Load VBT before devicetree-specific config. This only supports
	 * memory-mapped SPI at present.
	 */
	bootstage_start(BOOTSTAGE_ID_ACCUM_MMAP_SPI, "mmap_spi");
	memcpy(vbt_buf, (void *)vbt.image_pos, vbt.size);
	bootstage_accum(BOOTSTAGE_ID_ACCUM_MMAP_SPI);
	if (*(u32 *)vbt_buf != VBT_SIGNATURE)
		return log_msg_ret("VBT signature", -EINVAL);
#endif

	node = dev_read_subnode(dev, "fsp-s");
	if (!ofnode_valid(node))
		return log_msg_ret("fsp-s settings", -ENOENT);

	cfg->active_processor_cores = ofnode_read_bool(node,
					"fsps,enable-active-processor-cores");
	cfg->disable_core1 = !ofnode_read_bool(node, "fsps,enable-core1");
	cfg->disable_core2 = !ofnode_read_bool(node, "fsps,enable-core2");
	cfg->disable_core3 = !ofnode_read_bool(node, "fsps,enable-core3");
	cfg->vmx_enable = !ofnode_read_bool(node, "fsps,disable-vmx");
	cfg->proc_trace_mem_size = ofnode_read_u32_default(node,
						"fsps,proc-trace-mem-size",
						PROC_TRACE_MEM_SIZE_DISABLE);
	cfg->proc_trace_enable = ofnode_read_bool(node,
						  "fsps,enable-proc-trace");
	cfg->eist = !ofnode_read_bool(node, "fsps,disable-eist");
	cfg->boot_p_state =  ofnode_read_u32_default(node, "fsps,boot-p-state",
						     BOOT_P_STATE_HFM);
	cfg->enable_cx = !ofnode_read_bool(node, "fsps,disable-cx");
	cfg->c1e = ofnode_read_bool(node, "fsps,enable-c1e");
	cfg->bi_proc_hot = !ofnode_read_bool(node, "fsps,disable-bi-proc-hot");
	cfg->pkg_c_state_limit = ofnode_read_u32_default(node,
						"fsps,pkg-c-state-limit",
						PKG_C_STATE_LIMIT_C3);
	cfg->c_state_auto_demotion = ofnode_read_u32_default(node,
					"fsps,c-state-auto-demotion",
					C_STATE_AUTO_DEMOTION_DISABLE_C1_C3);
	cfg->c_state_un_demotion = ofnode_read_u32_default(node,
					"fsps,c-state-un-demotion",
					C_STATE_UN_DEMOTION_DISABLE_C1_C3);
	cfg->max_core_c_state = ofnode_read_u32_default(node,
					"fsps,max-core-c-state",
					MAX_CORE_C_STATE_CCX);
	cfg->pkg_c_state_demotion = ofnode_read_bool(node,
					"fsps,enable-pkg-c-state-demotion");
	cfg->pkg_c_state_un_demotion = !ofnode_read_bool(node,
					"fsps,disable-pkg-c-state-un-demotion");
	cfg->turbo_mode = !ofnode_read_bool(node, "fsps,disable-turbo-mode");
	cfg->hda_verb_table_entry_num = ofnode_read_u32_default(node,
					"fsps,hda-verb-table-entry-num",
					HDA_VERB_TABLE_ENTRY_NUM_DEFAULT);
	cfg->hda_verb_table_ptr = ofnode_read_u32_default(node,
						"fsps,hda-verb-table-ptr",
						0x00000000);
	cfg->p2sb_unhide = ofnode_read_bool(node, "fsps,enable-p2sb-unhide");
	cfg->ipu_en = !ofnode_read_bool(node, "fsps,disable-ipu");
	cfg->ipu_acpi_mode = ofnode_read_u32_default(node,
					"fsps,enable-ipu-acpi-mode",
					IPU_ACPI_MODE_IGFX_CHILD_DEVICE);
	cfg->force_wake = ofnode_read_bool(node, "fsps,enable-force-wake");
	cfg->gtt_mm_adr = ofnode_read_u32_default(node, "fsps,gtt-mm-adr",
						  GTT_MM_ADDRESS_DEFAULT);
	cfg->gm_adr = ofnode_read_u32_default(node, "fsps,gm-adr",
					      GM_ADDRESS_DEFAULT);
	cfg->pavp_lock = ofnode_read_bool(node, "fsps,enable-pavp-lock");
	cfg->graphics_freq_modify = ofnode_read_bool(node,
					"fsps,enable-graphics-freq-modify");
	cfg->graphics_freq_req = ofnode_read_bool(node,
					"fsps,enable-graphics-freq-req");
	cfg->graphics_video_freq = ofnode_read_bool(node,
					"fsps,enable-graphics-video-freq");
	cfg->pm_lock = ofnode_read_bool(node, "fsps,enable-pm-lock");
	cfg->dop_clock_gating = ofnode_read_bool(node,
					"fsps,enable-dop-clock-gating");
	cfg->unsolicited_attack_override = ofnode_read_bool(node,
				"fsps,enable-unsolicited-attack-override");
	cfg->wopcm_support = ofnode_read_bool(node,
				"fsps,enable-wopcm-support");
	cfg->wopcm_size = ofnode_read_bool(node, "fsps,enable-wopcm-size");
	cfg->power_gating = ofnode_read_bool(node, "fsps,enable-power-gating");
	cfg->unit_level_clock_gating = ofnode_read_bool(node,
					"fsps,enable-unit-level-clock-gating");
	cfg->fast_boot = ofnode_read_bool(node, "fsps,enable-fast-boot");
	cfg->dyn_sr = ofnode_read_bool(node, "fsps,enable-dyn-sr");
	cfg->sa_ipu_enable = ofnode_read_bool(node,
					      "fsps,enable-sa-ipu");
	cfg->pm_support = !ofnode_read_bool(node, "fsps,disable-pm-support");
	cfg->enable_render_standby = !ofnode_read_bool(node,
						"fsps,disable-render-standby");
	cfg->logo_size = ofnode_read_u32_default(node, "fsps,logo-size", 0);
	cfg->logo_ptr = ofnode_read_u32_default(node, "fsps,logo-ptr", 0);
#ifdef CONFIG_HAVE_VBT
	cfg->graphics_config_ptr = (ulong)vbt_buf;
#else
	cfg->graphics_config_ptr = 0;
#endif
	cfg->pavp_enable = !ofnode_read_bool(node, "fsps,disable-pavp");
	cfg->pavp_pr3 = !ofnode_read_bool(node, "fsps,disable-pavp-pr3");
	cfg->cd_clock = ofnode_read_u32_default(node, "fsps,cd-clock",
						CD_CLOCK_FREQ_624MHZ);
	cfg->pei_graphics_peim_init = !ofnode_read_bool(node,
					"fsps,disable-pei-graphics-peim-init");
	read_u8_array(cfg->write_protection_enable, node,
		      "fsps,enable-write-protection",
		      ARRAY_SIZE(cfg->write_protection_enable), 0);
	read_u8_array(cfg->read_protection_enable, node,
		      "fsps,enable-read-protection",
		      ARRAY_SIZE(cfg->read_protection_enable), 0);
	read_u16_array(cfg->protected_range_limit, node,
		       "fsps,protected-range-limit",
		       ARRAY_SIZE(cfg->protected_range_limit),
		       PROTECTED_RANGE_LIMITATION_DEFAULT);
	read_u16_array(cfg->protected_range_base, node,
		       "fsps,protected-range-base",
		       ARRAY_SIZE(cfg->protected_range_base), 0);
	cfg->gmm = !ofnode_read_bool(node, "fsps,disable-gmm");
	cfg->clk_gating_pgcb_clk_trunk = !ofnode_read_bool(node,
				"fsps,disable-clk-gating-pgcb-clk-trunk");
	cfg->clk_gating_sb = !ofnode_read_bool(node,
					       "fsps,disable-clk-gating-sb");
	cfg->clk_gating_sb_clk_trunk = !ofnode_read_bool(node,
					"fsps,disable-clk-gating-sb-clk-trunk");
	cfg->clk_gating_sb_clk_partition = !ofnode_read_bool(node,
				"fsps,disable-clk-gating-sb-clk-partition");
	cfg->clk_gating_core = !ofnode_read_bool(node,
					"fsps,disable-clk-gating-core");
	cfg->clk_gating_dma = !ofnode_read_bool(node,
						"fsps,disable-clk-gating-dma");
	cfg->clk_gating_reg_access = !ofnode_read_bool(node,
					"fsps,disable-clk-gating-reg-access");
	cfg->clk_gating_host = !ofnode_read_bool(node,
					"fsps,disable-clk-gating-host");
	cfg->clk_gating_partition = !ofnode_read_bool(node,
					"fsps,disable-clk-gating-partition");
	cfg->clk_gating_trunk = !ofnode_read_bool(node,
					"fsps,disable-clk-gating-trunk");
	cfg->hda_enable = !ofnode_read_bool(node, "fsps,disable-hda");
	cfg->dsp_enable = !ofnode_read_bool(node, "fsps,disable-dsp");
	cfg->pme = ofnode_read_bool(node, "fsps,enable-pme");
	cfg->hd_audio_io_buffer_ownership = ofnode_read_u32_default(node,
					"fsps,hd-audio-io-buffer-ownership",
					HDA_IO_BUFFER_OWNERSHIP_HDA_ALL_IO);
	cfg->hd_audio_io_buffer_voltage = ofnode_read_u32_default(node,
					"fsps,hd-audio-io-buffer-voltage",
					HDA_IO_BUFFER_VOLTAGE_3V3);
	cfg->hd_audio_vc_type = ofnode_read_u32_default(node,
							"fsps,hd-audio-vc-type",
							HDA_VC_TYPE_VC0);
	cfg->hd_audio_link_frequency = ofnode_read_u32_default(node,
						"fsps,hd-audio-link-frequency",
						HDA_LINK_FREQ_6MHZ);
	cfg->hd_audio_i_disp_link_frequency = ofnode_read_u32_default(node,
					"fsps,hd-audio-i-disp-link-frequency",
					HDA_I_DISP_LINK_FREQ_6MHZ);
	cfg->hd_audio_i_disp_link_tmode = ofnode_read_u32_default(node,
					"fsps,hd-audio-i-disp-link-tmode",
					HDA_I_DISP_LINK_T_MODE_2T);
	cfg->dsp_endpoint_dmic = ofnode_read_u32_default(node,
						"fsps,dsp-endpoint-dmic",
						HDA_DISP_DMIC_2CH_ARRAY);
	cfg->dsp_endpoint_bluetooth = !ofnode_read_bool(node,
					"fsps,disable-dsp-endpoint-bluetooth");
	cfg->dsp_endpoint_i2s_skp = ofnode_read_bool(node,
					"fsps,enable-dsp-endpoint-i2s-skp");
	cfg->dsp_endpoint_i2s_hp = ofnode_read_bool(node,
					"fsps,enable-dsp-endpoint-i2s-hp");
	cfg->audio_ctl_pwr_gate = ofnode_read_bool(node,
					"fsps,enable-audio-ctl-pwr-gate");
	cfg->audio_dsp_pwr_gate = ofnode_read_bool(node,
					"fsps,enable-audio-dsp-pwr-gate");
	cfg->mmt = ofnode_read_u32_default(node, "fsps,mmt",
					   HDA_CSE_MEM_TRANSFERS_VC0);
	cfg->hmt = ofnode_read_u32_default(node, "fsps,hmt",
					   HDA_HOST_MEM_TRANSFERS_VC0);
	cfg->hd_audio_pwr_gate = ofnode_read_bool(node,
					"fsps,enable-hd-audio-pwr-gate");
	cfg->hd_audio_clk_gate = ofnode_read_bool(node,
					"fsps,enable-hd-audio-clk-gate");
	cfg->dsp_feature_mask = ofnode_read_u32_default(node,
							"fsps,dsp-feature-mask",
							0x00000000);
	cfg->dsp_pp_module_mask = ofnode_read_u32_default(node,
						"fsps,dsp-pp-module-mask",
						 0x00000000);
	cfg->bios_cfg_lock_down = ofnode_read_bool(node,
					"fsps,enable-bios-cfg-lock-down");
	cfg->hpet = !ofnode_read_bool(node, "fsps,disable-hpet");
	cfg->hpet_bdf_valid = ofnode_read_bool(node,
					       "fsps,enable-hpet-bdf-valid");
	cfg->hpet_bus_number = ofnode_read_u32_default(node,
						       "fsps,hpet-bus-number",
						       HPET_BUS_NUMBER_DEFAULT);
	cfg->hpet_device_number = ofnode_read_u32_default(node,
						"fsps,hpet-device-number",
						HPET_DEVICE_NUMBER_DEFAULT);
	cfg->hpet_function_number = ofnode_read_u32_default(node,
						"fsps,hpet-function-number",
						HPET_FUNCTION_NUMBER_DEFAULT);
	cfg->io_apic_bdf_valid = ofnode_read_bool(node,
					       "fsps,enable-io-apic-bdf-valid");
	cfg->io_apic_bus_number = ofnode_read_u32_default(node,
						"fsps,io-apic-bus-number",
						IOAPIC_BUS_NUMBER_DEFAULT);
	cfg->io_apic_device_number = ofnode_read_u32_default(node,
						"fsps,io-apic-device-number",
						IOAPIC_DEVICE_NUMBER_DEFAULT);
	cfg->io_apic_function_number = ofnode_read_u32_default(node,
						"fsps,io-apic-function-number",
						IOAPIC_FUNCTION_NUMBER_DEFAULT);
	cfg->io_apic_entry24_119 = !ofnode_read_bool(node,
					"fsps,disable-io-apic-entry24-119");
	cfg->io_apic_id = ofnode_read_u32_default(node,
						  "fsps,io-apic-id",
						  IOAPIC_ID_DEFAULT);
	cfg->io_apic_range_select = ofnode_read_u32_default(node,
						  "fsps,io-apic-range-select",
						  0);
	cfg->ish_enable = !ofnode_read_bool(node, "fsps,disable-ish");
	cfg->bios_interface = !ofnode_read_bool(node,
					"fsps,disable-bios-interface");
	cfg->bios_lock = ofnode_read_bool(node, "fsps,enable-bios-lock");
	cfg->spi_eiss = !ofnode_read_bool(node, "fsps,disable-spi-eiss");
	cfg->bios_lock_sw_smi_number = ofnode_read_u32_default(node,
					"fsps,bios-lock-sw-smi-number",
					BIOS_LOCK_SW_SMI_NUMBER_DEFAULT);
	cfg->lpss_s0ix_enable = ofnode_read_bool(node, "fsps,enable-lpss-s0ix");
	read_u8_array(cfg->i2c_clk_gate_cfg, node,
		      "fsps,i2c-clk-gate-cfg",
		      sizeof(cfg->i2c_clk_gate_cfg), 1);
	read_u8_array(cfg->hsuart_clk_gate_cfg, node,
		      "fsps,hsuart-clk-gate-cfg",
		      sizeof(cfg->hsuart_clk_gate_cfg), 1);
	read_u8_array(cfg->spi_clk_gate_cfg, node,
		      "fsps,spi-clk-gate-cfg",
		      sizeof(cfg->spi_clk_gate_cfg), 1);
	for (int i = 0; i < FSP_I2C_COUNT; i++)  {
		snprintf(buf, sizeof(buf), "fsps,enable-i2c%d", i);
		*(&cfg->i2c0_enable + i) = ofnode_read_u32_default(node, buf,
							I2CX_ENABLE_PCI_MODE);
	}
	for (int i = 0; i < FSP_HSUART_COUNT; i++)  {
		snprintf(buf, sizeof(buf), "fsps,enable-hsuart%d", i);
		*(&cfg->hsuart0_enable + i) = ofnode_read_u32_default(node, buf,
						HSUARTX_ENABLE_PCI_MODE);
	}
	for (int i = 0; i < FSP_SPI_COUNT; i++)  {
		snprintf(buf, sizeof(buf), "fsps,enable-spi%d", i);
		*(&cfg->spi0_enable + i) = ofnode_read_u32_default(node, buf,
							SPIX_ENABLE_PCI_MODE);
	}
	cfg->os_dbg_enable = ofnode_read_bool(node, "fsps,enable-os-dbg");
	cfg->dci_en = ofnode_read_bool(node, "fsps,enable-dci");
	cfg->uart2_kernel_debug_base_address = ofnode_read_u32_default(node,
					"fsps,uart2-kernel-debug-base-address",
					0);
	cfg->pcie_clock_gating_disabled = !ofnode_read_bool(node,
					"fsps,disable-pcie-clock-gating");
	cfg->pcie_root_port8xh_decode = !ofnode_read_bool(node,
				"fsps,disable-pcie-root-port8xh-decode");
	cfg->pcie8xh_decode_port_index = ofnode_read_u32_default(node,
					"fsps,pcie8xh-decode-port-index",
					0);
	cfg->pcie_root_port_peer_memory_write_enable = ofnode_read_bool(node,
				"fsps,enable-pcie-root-port-peer-memory-write");
	cfg->pcie_aspm_sw_smi_number = ofnode_read_u32_default(node,
					"fsps,pcie-aspm-sw-smi-number",
					PCIE_ASPM_SW_SMI_NUMBER_DEFAULT);
	read_u8_array(cfg->pcie_root_port_en, node,
		      "fsps,enable-pcie-root-port",
		      ARRAY_SIZE(cfg->pcie_root_port_en), 1);
	read_u8_array(cfg->pcie_rp_hide, node, "fsps,pcie-rp-hide",
		      ARRAY_SIZE(cfg->pcie_rp_hide), 0);
	read_u8_array(cfg->pcie_rp_slot_implemented, node,
		      "fsps,pcie-rp-slot-implemented",
		      ARRAY_SIZE(cfg->pcie_rp_slot_implemented), 1);
	read_u8_array(cfg->pcie_rp_hot_plug, node, "fsps,pcie-rp-hot-plug",
		      ARRAY_SIZE(cfg->pcie_rp_hot_plug), 1);
	read_u8_array(cfg->pcie_rp_pm_sci, node, "fsps,pcie-rp-pm-sci",
		      ARRAY_SIZE(cfg->pcie_rp_pm_sci), 0);
	read_u8_array(cfg->pcie_rp_ext_sync, node, "fsps,pcie-rp-ext-sync",
		      ARRAY_SIZE(cfg->pcie_rp_ext_sync), 1);
	read_u8_array(cfg->pcie_rp_transmitter_half_swing, node,
		      "fsps,pcie-rp-transmitter-half-swing",
		      ARRAY_SIZE(cfg->pcie_rp_transmitter_half_swing), 1);
	read_u8_array(cfg->pcie_rp_acs_enabled, node,
		      "fsps,pcie-rp-acs",
		      ARRAY_SIZE(cfg->pcie_rp_acs_enabled), 1);
	read_u8_array(cfg->pcie_rp_clk_req_supported, node,
		      "fsps,pcie-rp-clk-req-supported",
		      ARRAY_SIZE(cfg->pcie_rp_clk_req_supported), 1);
	ret = read_u8_array(cfg->pcie_rp_clk_req_number, node,
		      "fsps,pcie-rp-clk-req-number",
		      ARRAY_SIZE(cfg->pcie_rp_clk_req_number), 1);
	if (ret)
		memcpy(cfg->pcie_rp_clk_req_number, pcie_rp_clk_req_number_def,
		       sizeof(cfg->pcie_rp_clk_req_number));
	read_u8_array(cfg->pcie_rp_clk_req_detect, node,
		      "fsps,pcie-rp-clk-req-detect",
		      ARRAY_SIZE(cfg->pcie_rp_clk_req_detect), 0);
	read_u8_array(cfg->advanced_error_reporting, node,
		      "fsps,advanced-error-reportingt",
		      ARRAY_SIZE(cfg->advanced_error_reporting), 0);
	read_u8_array(cfg->pme_interrupt, node, "fsps,pme-interrupt",
		      ARRAY_SIZE(cfg->pme_interrupt), 0);
	read_u8_array(cfg->unsupported_request_report, node,
		      "unsupported-request-report",
		      ARRAY_SIZE(cfg->unsupported_request_report), 0);
	read_u8_array(cfg->fatal_error_report, node,
		      "fsps,fatal-error-report",
		      ARRAY_SIZE(cfg->fatal_error_report), 0);
	read_u8_array(cfg->no_fatal_error_report, node,
		      "fsps,no-fatal-error-report",
		      ARRAY_SIZE(cfg->no_fatal_error_report), 0);
	read_u8_array(cfg->correctable_error_report, node,
		      "fsps,correctable-error-report",
		      ARRAY_SIZE(cfg->correctable_error_report), 0);
	read_u8_array(cfg->system_error_on_fatal_error, node,
		      "fsps,system-error-on-fatal-error",
		      ARRAY_SIZE(cfg->system_error_on_fatal_error), 0);
	read_u8_array(cfg->system_error_on_non_fatal_error, node,
		      "fsps,system-error-on-non-fatal-error",
		      ARRAY_SIZE(cfg->system_error_on_non_fatal_error), 0);
	read_u8_array(cfg->system_error_on_correctable_error, node,
		      "fsps,system-error-on-correctable-error",
		      ARRAY_SIZE(cfg->system_error_on_correctable_error), 0);
	read_u8_array(cfg->pcie_rp_speed, node, "fsps,pcie-rp-speed",
		      ARRAY_SIZE(cfg->pcie_rp_speed), PCIE_RP_SPEED_AUTO);
	ret = read_u8_array(cfg->physical_slot_number, node,
			    "fsps,physical-slot-number",
			    ARRAY_SIZE(cfg->physical_slot_number), 0);
	if (ret)
		memcpy(cfg->physical_slot_number, physical_slot_number_def,
		       sizeof(cfg->physical_slot_number));
	read_u8_array(cfg->pcie_rp_completion_timeout, node,
		      "fsps,pcie-rp-completion-timeout",
		      ARRAY_SIZE(cfg->pcie_rp_completion_timeout), 0);
	read_u8_array(cfg->ptm_enable, node, "fsps,enable-ptm",
		      ARRAY_SIZE(cfg->ptm_enable), 0);
	read_u8_array(cfg->pcie_rp_aspm, node, "fsps,pcie-rp-aspm",
		      ARRAY_SIZE(cfg->pcie_rp_aspm), PCIE_RP_ASPM_AUTO);
	read_u8_array(cfg->pcie_rp_l1_substates, node,
		      "fsps,pcie-rp-l1-substates",
		      ARRAY_SIZE(cfg->pcie_rp_l1_substates),
		      PCIE_RP_L1_SUBSTATES_L1_1_L1_2);
	read_u8_array(cfg->pcie_rp_ltr_enable, node,
		      "fsps,pcie-rp-ltr-enable",
		      ARRAY_SIZE(cfg->pcie_rp_ltr_enable), 1);
	read_u8_array(cfg->pcie_rp_ltr_config_lock, node,
		      "fsps,pcie-rp-ltr-config-lock",
		      ARRAY_SIZE(cfg->pcie_rp_ltr_config_lock), 0);
	cfg->pme_b0_s5_dis = ofnode_read_bool(node,
					       "fsps,disable-pme-b0-s5");
	cfg->pci_clock_run = ofnode_read_bool(node,
					       "fsps,enable-pci-clock-run");
	cfg->timer8254_clk_setting = ofnode_read_bool(node,
					"fsps,enable-timer8254-clk-setting");
	cfg->enable_sata = !ofnode_read_bool(node, "fsps,disable-sata");
	cfg->sata_mode = ofnode_read_u32_default(node, "fsps,sata-mode",
						 SATA_MODE_AHCI);
	cfg->sata_salp_support = !ofnode_read_bool(node,
					"fsps,disable-sata-salp-support");
	cfg->sata_pwr_opt_enable = ofnode_read_bool(node,
					"fsps,enable-sata-pwr-opt");
	cfg->e_sata_speed_limit = ofnode_read_bool(node,
					"fsps,enable-e-sata-speed-limit");
	cfg->speed_limit = ofnode_read_u32_default(node, "fsps,speed-limit",
						SATA_SPEED_LIMIT_SC_SATA_SPEED);
	read_u8_array(cfg->sata_ports_enable, node,
		      "fsps,enable-sata-ports",
		      ARRAY_SIZE(cfg->sata_ports_enable), 1);
	read_u8_array(cfg->sata_ports_dev_slp, node,
		      "fsps,sata-ports-dev-slp",
		      ARRAY_SIZE(cfg->sata_ports_dev_slp), 0);
	read_u8_array(cfg->sata_ports_hot_plug, node,
		      "fsps,sata-ports-hot-plug",
		      ARRAY_SIZE(cfg->sata_ports_hot_plug), 0);
	read_u8_array(cfg->sata_ports_interlock_sw, node,
		      "fsps,sata-ports-interlock-sw",
		      ARRAY_SIZE(cfg->sata_ports_interlock_sw), 1);
	read_u8_array(cfg->sata_ports_external, node,
		      "fsps,sata-ports-external",
		      ARRAY_SIZE(cfg->sata_ports_external), 0);
	read_u8_array(cfg->sata_ports_spin_up, node,
		      "fsps,sata-ports-spin-up",
		      ARRAY_SIZE(cfg->sata_ports_spin_up), 0);
	read_u8_array(cfg->sata_ports_solid_state_drive, node,
		      "fsps,sata-ports-solid-state-drive",
		      ARRAY_SIZE(cfg->sata_ports_solid_state_drive),
		      SATA_PORT_SOLID_STATE_DRIVE_HARD_DISK_DRIVE);
	read_u8_array(cfg->sata_ports_enable_dito_config, node,
		      "fsps,enable-sata-ports-dito-config",
		      ARRAY_SIZE(cfg->sata_ports_enable_dito_config), 0);
	read_u8_array(cfg->sata_ports_dm_val, node,
		      "fsps,sata-ports-dm-val",
		      ARRAY_SIZE(cfg->sata_ports_dm_val),
		      SATA_PORTS_DM_VAL_DEFAULT);
	read_u16_array(cfg->sata_ports_dito_val, node,
		      "fsps,sata-ports-dito-val",
		      ARRAY_SIZE(cfg->sata_ports_dito_val),
		      SATA_PORTS_DITO_VAL_DEFAULT);
	cfg->sub_system_vendor_id = ofnode_read_u32_default(node,
						"fsps,sub-system-vendor-id",
						PCI_VENDOR_ID_INTEL);
	cfg->sub_system_id = ofnode_read_u32_default(node,
						"fsps,sub-system-id",
						SUB_SYSTEM_ID_DEFAULT);
	cfg->crid_settings = ofnode_read_u32_default(node,
						"fsps,crid-settings",
						CRID_SETTING_DISABLE);
	cfg->reset_select = ofnode_read_u32_default(node,
						"fsps,reset-select",
						RESET_SELECT_WARM_RESET);
	cfg->sdcard_enabled = !ofnode_read_bool(node, "fsps,disable-sdcard");
	cfg->e_mmc_enabled = !ofnode_read_bool(node, "fsps,disable-emmc");
	cfg->e_mmc_host_max_speed = ofnode_read_u32_default(node,
						"fsps,emmc-host-max-speed",
						EMMC_HOST_SPEED_MAX_HS400);
	cfg->ufs_enabled = !ofnode_read_bool(node, "fsps,disable-ufs");
	cfg->sdio_enabled = !ofnode_read_bool(node, "fsps,disable-sdio");
	cfg->gpp_lock = ofnode_read_bool(node, "fsps,enable-gpp-lock");
	cfg->sirq_enable = !ofnode_read_bool(node, "fsps,disable-sirq");
	cfg->sirq_mode = ofnode_read_u32_default(node, "fsps,sirq-mode",
						SERIAL_IRQ_MODE_QUIET_MODE);
	cfg->start_frame_pulse = ofnode_read_u32_default(node,
					"fsps,start-frame-pulse",
					START_FRAME_PULSE_WIDTH_SCSFPW4CLK);
	cfg->smbus_enable = !ofnode_read_bool(node, "fsps,disable-smbus");
	cfg->arp_enable = !ofnode_read_bool(node, "fsps,disable-arp");
	cfg->num_rsvd_smbus_addresses = ofnode_read_u32_default(node,
					"fsps,num-rsvd-smbus-addresses",
					NUM_RSVD_SMBUS_ADDRESSES_DEFAULT);
	if (cfg->num_rsvd_smbus_addresses > 0) {
		read_u8_array(cfg->rsvd_smbus_address_table, node,
			      "fsps,rsvd-smbus-address-table",
			      cfg->num_rsvd_smbus_addresses, 0x0000);
	}
	cfg->disable_compliance_mode = ofnode_read_bool(node,
					"fsps,disable-compliance-mode");
	cfg->usb_per_port_ctl = ofnode_read_bool(node,
					"fsps,enable-usb-per-port-ctl");
	cfg->usb30_mode = ofnode_read_u32_default(node,
					"fsps,usb30-mode",
					USB30_MODE_AUTO);
	read_u8_array(cfg->port_usb20_enable, node,
		      "fsps,enable-port-usb20",
		      ARRAY_SIZE(cfg->port_usb20_enable), 1);
	read_u8_array(cfg->port_us20b_over_current_pin, node,
		      "fsps,port-usb20-over-current-pin",
		      ARRAY_SIZE(cfg->port_us20b_over_current_pin),
		      PORT_USB20_OVER_CURRENT_PIN_DEFAULT);
	cfg->usb_otg = ofnode_read_u32_default(node, "fsps,usb-otg",
					       USB_OTG_PCI_MODE);
	cfg->hsic_support_enable = ofnode_read_bool(node,
					"fsps,enable-hsic-support");
	read_u8_array(cfg->port_usb30_enable, node,
			      "fsps,enable-port-usb30",
			      ARRAY_SIZE(cfg->port_usb30_enable), 1);
	read_u8_array(cfg->port_us30b_over_current_pin, node,
		      "fsps,port-usb30-over-current-pin",
		      ARRAY_SIZE(cfg->port_us30b_over_current_pin),
		      PORT_USB30_OVER_CURRENT_PIN_DEFAULT);
	read_u8_array(cfg->ssic_port_enable, node,
			      "fsps,enable-ssic-port",
			      ARRAY_SIZE(cfg->ssic_port_enable),
			      0);
	cfg->dlane_pwr_gating = !ofnode_read_bool(node,
					"fsps,disable-dlane-pwr-gating");
	cfg->vtd_enable = ofnode_read_bool(node, "fsps,enable-vtd");
	cfg->lock_down_global_smi = !ofnode_read_bool(node,
					"fsps,disable-lock-down-global-smi");
	cfg->reset_wait_timer = ofnode_read_u32_default(node,
						"fsps,reset-wait-timer",
						RESET_WAIT_TIMER_DEFAULT);
	cfg->rtc_lock = !ofnode_read_bool(node, "fsps,disable-rtc-lock");
	cfg->sata_test_mode = ofnode_read_bool(node,
					       "fsps,enable-safe-test-mode");
	read_u8_array(cfg->ssic_rate, node,
				      "fsps,ssic-rate",
				      ARRAY_SIZE(cfg->ssic_rate),
				      SSIC_RATE_A_SERIES);
	cfg->dynamic_power_gating = ofnode_read_bool(node,
					"fsps,enable-dynamic-power-gating");
	read_u16_array(cfg->pcie_rp_ltr_max_snoop_latency, node,
			"fsps,pcie-rp-ltr-max-snoop-latency",
			ARRAY_SIZE(cfg->pcie_rp_ltr_max_snoop_latency),
			PCIE_RP_LTR_MAX_SNOOP_LATENCY_DEFAULT);
	read_u8_array(cfg->pcie_rp_snoop_latency_override_mode, node,
		      "fsps,pcie-rp-snoop-latency-override-mode",
		      ARRAY_SIZE(cfg->pcie_rp_snoop_latency_override_mode),
		      PCIE_RP_SNOOP_LATENCY_OVERRIDE_MODE_AUTO);
	read_u16_array(cfg->pcie_rp_snoop_latency_override_value, node,
		       "fsps,pcie-rp-snoop-latency-override-value",
		       ARRAY_SIZE(cfg->pcie_rp_snoop_latency_override_value),
		       PCIE_RP_SNOOP_LATENCY_OVERRIDE_VALUE_DEFAULT);
	read_u8_array(cfg->pcie_rp_snoop_latency_override_multiplier, node,
		"fsps,pcie-rp-snoop-latency-override-multiplier",
		ARRAY_SIZE(cfg->pcie_rp_snoop_latency_override_multiplier),
		PCIE_RP_SNOOP_LATENCY_OVERRIDE_MULTIPLIER_1024NS);
	cfg->skip_mp_init = ofnode_read_bool(node, "fsps,enable-skip-mp-init");
	cfg->dci_auto_detect = !ofnode_read_bool(node,
					"fsps,disable-dci-auto-detect");
	read_u16_array(cfg->pcie_rp_ltr_max_non_snoop_latency, node,
		       "fsps,pcie-rp-ltr-max-non-snoop-latency",
		       ARRAY_SIZE(cfg->pcie_rp_ltr_max_non_snoop_latency),
		       PCIE_RP_LTR_MAX_NON_SNOOP_LATENCY_DEFAULT);
	read_u8_array(cfg->pcie_rp_non_snoop_latency_override_mode, node,
		      "fsps,pcie-rp-non-snoop-latency-override-mode",
		      ARRAY_SIZE(cfg->pcie_rp_non_snoop_latency_override_mode),
		      PCIE_RP_NON_SNOOP_LATENCY_OVERRIDE_MODE_AUTO);
	cfg->tco_timer_halt_lock = !ofnode_read_bool(node,
					"fsps,disable-tco-timer-halt-lock");
	cfg->pwr_btn_override_period = ofnode_read_u32_default(node,
					"fsps,pwr-btn-override-period",
					PWR_BTN_OVERRIDE_PERIOD_4S);
	read_u16_array(cfg->pcie_rp_non_snoop_latency_override_value, node,
		"fsps,pcie-rp-non-snoop-latency-override-value",
		ARRAY_SIZE(cfg->pcie_rp_non_snoop_latency_override_value),
		PCIE_RP_NON_SNOOP_LATENCY_OVERRIDE_VALUE_DEFAULT);
	read_u8_array(cfg->pcie_rp_non_snoop_latency_override_multiplier, node,
		"fsps,pcie-rp-non-snoop-latency-override-multiplier",
		ARRAY_SIZE(cfg->pcie_rp_non_snoop_latency_override_multiplier),
		PCIE_RP_NON_SNOOP_LATENCY_OVERRIDE_MULTIPLIER_1024NS);
	read_u8_array(cfg->pcie_rp_slot_power_limit_scale, node,
		      "fsps,pcie-rp-slot-power-limit-scale",
		      ARRAY_SIZE(cfg->pcie_rp_slot_power_limit_scale),
		      PCIE_RP_SLOT_POWER_LIMIT_SCALE_DEFAULT);
	read_u8_array(cfg->pcie_rp_slot_power_limit_value, node,
		      "fsps,pcie-rp-slot-power-limit-value",
		      ARRAY_SIZE(cfg->pcie_rp_slot_power_limit_value),
		      PCIE_RP_SLOT_POWER_LIMIT_VALUE_DEFAULT);
	cfg->disable_native_power_button = ofnode_read_bool(node,
					"fsps,disable-native-power-button");
	cfg->power_butter_debounce_mode = !ofnode_read_bool(node,
				"fsps,disable-power-button-debounce-mode");
	cfg->sdio_tx_cmd_cntl = ofnode_read_u32_default(node,
						"fsps,sdio-tx-cmd-cntl",
						SDIO_TX_CMD_CNTL_DEFAULT);
	cfg->sdio_tx_data_cntl1 = ofnode_read_u32_default(node,
						"fsps,sdio-tx-data-cntl1",
						SDIO_TX_DATA_CNTL1_DEFAULT);
	cfg->sdio_tx_data_cntl2 = ofnode_read_u32_default(node,
						"fsps,sdio-tx-data-cntl2",
						SDIO_TX_DATA_CNTL2_DEFAULT);
	cfg->sdio_rx_cmd_data_cntl1 = ofnode_read_u32_default(node,
						"fsps,sdio-rx-cmd-data-cntl1",
						SDIO_RX_CMD_DATA_CNTL1_DEFAULT);
	cfg->sdio_rx_cmd_data_cntl2 = ofnode_read_u32_default(node,
						"fsps,sdio-rx-cmd-data-cntl2",
						SDIO_RX_CMD_DATA_CNTL2_DEFAULT);
	cfg->sdcard_tx_cmd_cntl = ofnode_read_u32_default(node,
						"fsps,sdcard-tx-cmd-cntl",
						SDCARD_TX_CMD_CNTL_DEFAULT);
	cfg->sdcard_tx_data_cntl1 = ofnode_read_u32_default(node,
						"fsps,sdcard-tx-data-cntl1",
						SDCARD_TX_DATA_CNTL1_DEFAULT);
	cfg->sdcard_tx_data_cntl2 = ofnode_read_u32_default(node,
						"fsps,sdcard-tx-data-cntl2",
						SDCARD_TX_DATA_CNTL2_DEFAULT);
	cfg->sdcard_rx_cmd_data_cntl1 = ofnode_read_u32_default(node,
					"fsps,sdcard-rx-cmd-data-cntl1",
					SDCARD_RX_CMD_DATA_CNTL1_DEFAULT);
	cfg->sdcard_rx_strobe_cntl = ofnode_read_u32_default(node,
						"fsps,sdcard-rx-strobe-cntl",
						SDCARD_RX_STROBE_CNTL_DEFAULT);
	cfg->sdcard_rx_cmd_data_cntl2 = ofnode_read_u32_default(node,
					"fsps,sdcard-rx-cmd-data-cntl2",
					SDCARD_RX_CMD_DATA_CNTL2_DEFAULT);
	cfg->emmc_tx_cmd_cntl = ofnode_read_u32_default(node,
						"fsps,emmc-tx-cmd-cntl",
						EMMC_TX_CMD_CNTL_DEFAULT);
	cfg->emmc_tx_data_cntl1 = ofnode_read_u32_default(node,
						"fsps,emmc-tx-data-cntl1",
						EMMC_TX_DATA_CNTL1_DEFAULT);
	cfg->emmc_tx_data_cntl2 = ofnode_read_u32_default(node,
						"fsps,emmc-tx-data-cntl2",
						EMMC_TX_DATA_CNTL2_DEFAULT);
	cfg->emmc_rx_cmd_data_cntl1 = ofnode_read_u32_default(node,
						"fsps,emmc-rx-cmd-data-cntl1",
						EMMC_RX_CMD_DATA_CNTL1_DEFAULT);
	cfg->emmc_rx_strobe_cntl = ofnode_read_u32_default(node,
						"fsps,emmc-rx-strobe-cntl",
						EMMC_RX_STROBE_CNTL_DEFAULT);
	cfg->emmc_rx_cmd_data_cntl2 = ofnode_read_u32_default(node,
						"fsps,emmc-rx-cmd-data-cntl2",
						EMMC_RX_CMD_DATA_CNTL2_DEFAULT);
	cfg->emmc_master_sw_cntl = ofnode_read_u32_default(node,
						"fsps,emmc-master-sw-cntl",
						EMMC_MASTER_SW_CNTL_DEFAULT);
	read_u8_array(cfg->pcie_rp_selectable_deemphasis, node,
		      "fsps,pcie-rp-selectable-deemphasis",
		      ARRAY_SIZE(cfg->pcie_rp_selectable_deemphasis),
		      PCIE_RP_SELECTABLE_DEEMPHASIS_3_5_DB);
	cfg->monitor_mwait_enable = !ofnode_read_bool(node,
						"fsps,disable-monitor-mwait");
	cfg->hd_audio_dsp_uaa_compliance = ofnode_read_bool(node,
				"fsps,enable-hd-audio-dsp-uaa-compliance");
	ret = read_u32_array(cfg->ipc, node, "fsps,ipc", ARRAY_SIZE(cfg->ipc),
			     0);
	if (ret)
		memcpy(cfg->ipc, ipc_def, sizeof(cfg->ipc));

	read_u8_array(cfg->sata_ports_disable_dynamic_pg, node,
		      "fsps,sata-ports-disable-dynamic-pg",
		      ARRAY_SIZE(cfg->sata_ports_disable_dynamic_pg),
		      0);
	cfg->init_s3_cpu = ofnode_read_bool(node, "fsps,enable-init-s3-cpu");
	cfg->skip_punit_init = ofnode_read_bool(node,
						"fsps,enable-skip-punit-init");
	read_u8_array(cfg->port_usb20_per_port_tx_pe_half, node,
		      "fsps,port-usb20-per-port-tx-pe-half",
		      ARRAY_SIZE(cfg->port_usb20_per_port_tx_pe_half),
		      0);
	ret = read_u8_array(cfg->port_usb20_per_port_pe_txi_set, node,
			    "fsps,port-usb20-per-port-pe-txi-set",
			    ARRAY_SIZE(cfg->port_usb20_per_port_pe_txi_set),
			    0);
	if (ret)
		memcpy(cfg->port_usb20_per_port_pe_txi_set,
		       port_usb20_per_port_pe_txi_set_def,
		       sizeof(cfg->port_usb20_per_port_pe_txi_set));
	ret = read_u8_array(cfg->port_usb20_per_port_txi_set, node,
			    "fsps,port-usb20-per-port-txi-set",
			    ARRAY_SIZE(cfg->port_usb20_per_port_txi_set),
			    0);
	if (ret)
		memcpy(cfg->port_usb20_per_port_txi_set,
		       port_usb20_per_port_txi_set_def,
		       sizeof(cfg->port_usb20_per_port_txi_set));
	ret = read_u8_array(cfg->port_usb20_hs_skew_sel, node,
			    "fsps,port-usb20-hs-skew-sel",
			    ARRAY_SIZE(cfg->port_usb20_hs_skew_sel),
			    0);
	if (ret)
		memcpy(cfg->port_usb20_hs_skew_sel,
		       port_usb20_hs_skew_sel_def,
		       sizeof(cfg->port_usb20_hs_skew_sel));
	ret = read_u8_array(cfg->port_usb20_i_usb_tx_emphasis_en, node,
			    "fsps,port-usb20-i-usb-tx-emphasis-en",
			    ARRAY_SIZE(cfg->port_usb20_i_usb_tx_emphasis_en),
			    0);
	if (ret)
		memcpy(cfg->port_usb20_i_usb_tx_emphasis_en,
		       port_usb20_i_usb_tx_emphasis_en_def,
		       sizeof(cfg->port_usb20_i_usb_tx_emphasis_en));
	read_u8_array(cfg->port_usb20_per_port_rxi_set, node,
		      "fsps,port-usb20-per-port-rxi-set",
		      ARRAY_SIZE(cfg->port_usb20_per_port_rxi_set),
		      0);
	ret = read_u8_array(cfg->port_usb20_hs_npre_drv_sel, node,
			    "fsps,port-usb20-hs-npre-drv-sel",
			    ARRAY_SIZE(cfg->port_usb20_hs_npre_drv_sel),
			    0);
	if (ret)
		memcpy(cfg->port_usb20_hs_npre_drv_sel,
		       port_usb20_hs_npre_drv_sel_def,
		       sizeof(cfg->port_usb20_hs_npre_drv_sel));

	return 0;
}

static void p2sb_set_hide_bit(pci_dev_t dev, int hide)
{
	pci_x86_clrset_config(dev, PCH_P2SB_E0 + 1, HIDE_BIT,
			      hide ? HIDE_BIT : 0, PCI_SIZE_8);
}

/* Configure package power limits */
static int set_power_limits(struct udevice *dev)
{
	msr_t rapl_msr_reg, limit;
	u32 power_unit;
	u32 tdp, min_power, max_power;
	u32 pl2_val;
	u32 override_tdp[2];
	int ret;

	/* Get units */
	rapl_msr_reg = msr_read(MSR_PKG_POWER_SKU_UNIT);
	power_unit = 1 << (rapl_msr_reg.lo & 0xf);

	/* Get power defaults for this SKU */
	rapl_msr_reg = msr_read(MSR_PKG_POWER_SKU);
	tdp = rapl_msr_reg.lo & PKG_POWER_LIMIT_MASK;
	pl2_val = rapl_msr_reg.hi & PKG_POWER_LIMIT_MASK;
	min_power = (rapl_msr_reg.lo >> 16) & PKG_POWER_LIMIT_MASK;
	max_power = rapl_msr_reg.hi & PKG_POWER_LIMIT_MASK;

	if (min_power > 0 && tdp < min_power)
		tdp = min_power;

	if (max_power > 0 && tdp > max_power)
		tdp = max_power;

	ret = dev_read_u32_array(dev, "tdp-pl-override-mw", override_tdp,
				 ARRAY_SIZE(override_tdp));
	if (ret)
		return log_msg_ret("tdp-pl-override-mw", ret);

	/* Set PL1 override value */
	if (override_tdp[0])
		tdp = override_tdp[0] * power_unit / 1000;

	/* Set PL2 override value */
	if (override_tdp[1])
		pl2_val = override_tdp[1] * power_unit / 1000;

	/* Set long term power limit to TDP */
	limit.lo = tdp & PKG_POWER_LIMIT_MASK;
	/* Set PL1 Pkg Power clamp bit */
	limit.lo |= PKG_POWER_LIMIT_CLAMP;

	limit.lo |= PKG_POWER_LIMIT_EN;
	limit.lo |= (MB_POWER_LIMIT1_TIME_DEFAULT &
		PKG_POWER_LIMIT_TIME_MASK) << PKG_POWER_LIMIT_TIME_SHIFT;

	/* Set short term power limit PL2 */
	limit.hi = pl2_val & PKG_POWER_LIMIT_MASK;
	limit.hi |= PKG_POWER_LIMIT_EN;

	/* Program package power limits in RAPL MSR */
	msr_write(MSR_PKG_POWER_LIMIT, limit);
	log_info("RAPL PL1 %d.%dW\n", tdp / power_unit,
		 100 * (tdp % power_unit) / power_unit);
	log_info("RAPL PL2 %d.%dW\n", pl2_val / power_unit,
		 100 * (pl2_val % power_unit) / power_unit);

	/*
	 * Sett RAPL MMIO register for Power limits. RAPL driver is using MSR
	 * instead of MMIO, so disable LIMIT_EN bit for MMIO
	 */
	writel(limit.lo & ~PKG_POWER_LIMIT_EN, MCHBAR_REG(MCHBAR_RAPL_PPL));
	writel(limit.hi & ~PKG_POWER_LIMIT_EN, MCHBAR_REG(MCHBAR_RAPL_PPL + 4));

	return 0;
}

int p2sb_unhide(void)
{
	pci_dev_t dev = PCI_BDF(0, 0xd, 0);
	ulong val;

	p2sb_set_hide_bit(dev, 0);

	pci_x86_read_config(dev, PCI_VENDOR_ID, &val, PCI_SIZE_16);

	if (val != PCI_VENDOR_ID_INTEL)
		return log_msg_ret("p2sb unhide", -EIO);

	return 0;
}

/* Overwrites the SCI IRQ if another IRQ number is given by device tree */
static void set_sci_irq(void)
{
	/* Skip this for now */
}

int arch_fsps_preinit(void)
{
	struct udevice *itss;
	int ret;

	ret = irq_first_device_type(X86_IRQT_ITSS, &itss);
	if (ret)
		return log_msg_ret("no itss", ret);
	/*
	 * Snapshot the current GPIO IRQ polarities. FSP is setting a default
	 * policy that doesn't honour boards' requirements
	 */
	irq_snapshot_polarities(itss);

	/*
	 * Clear the GPI interrupt status and enable registers. These
	 * registers do not get reset to default state when booting from S5.
	 */
	ret = pinctrl_gpi_clear_int_cfg();
	if (ret)
		return log_msg_ret("gpi_clear", ret);

	return 0;
}

int arch_fsp_init_r(void)
{
#ifdef CONFIG_HAVE_ACPI_RESUME
	bool s3wake = gd->arch.prev_sleep_state == ACPI_S3;
#else
	bool s3wake = false;
#endif
	struct udevice *dev, *itss;
	int ret;

	/*
	 * This must be called before any devices are probed. Put any probing
	 * into arch_fsps_preinit() above.
	 *
	 * We don't use CONFIG_APL_BOOT_FROM_FAST_SPI_FLASH here since it will
	 * force PCI to be probed.
	 */
	ret = fsp_silicon_init(s3wake, false);
	if (ret)
		return ret;

	ret = irq_first_device_type(X86_IRQT_ITSS, &itss);
	if (ret)
		return log_msg_ret("no itss", ret);
	/* Restore GPIO IRQ polarities back to previous settings */
	irq_restore_polarities(itss);

	/* soc_init() */
	ret = p2sb_unhide();
	if (ret)
		return log_msg_ret("unhide p2sb", ret);

	/* Set RAPL MSR for Package power limits*/
	ret = uclass_first_device_err(UCLASS_NORTHBRIDGE, &dev);
	if (ret)
		return log_msg_ret("Cannot get northbridge", ret);
	set_power_limits(dev);

	/*
	 * FSP-S routes SCI to IRQ 9. With the help of this function you can
	 * select another IRQ for SCI.
	 */
	set_sci_irq();

	return 0;
}
