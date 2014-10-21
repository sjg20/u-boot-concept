/*
 * Copyright (c) 2011 The Chromium OS Authors.
 * (C) Copyright 2010,2011
 * Graeme Russ, <graeme.russ@gmail.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <errno.h>
#include <fdtdec.h>
#include <asm/cmos.h>
#include <asm/ip_checksum.h>
#include <asm/processor.h>
#include <asm/gpio.h>
#include <asm/global_data.h>
#include <asm/arch/me.h>
#include <asm/arch/pei_data.h>
#include <asm/arch/pch.h>
#include <asm/post.h>
#include <asm/arch/sandybridge.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * This function looks for the highest region of memory lower than 4GB which
 * has enough space for U-Boot where U-Boot is aligned on a page boundary. It
 * overrides the default implementation found elsewhere which simply picks the
 * end of ram, wherever that may be. The location of the stack, the relocation
 * address, and how far U-Boot is moved by relocation are set in the global
 * data structure.
 */
ulong board_get_usable_ram_top(ulong total_size)
{
	uintptr_t dest_addr = 0;

	/* TODO: Figure out memory size */
	dest_addr = gd->bd->bi_dram[0].start + gd->bd->bi_dram[0].size;

	/* If no suitable area was found, return an error. */
	if (!dest_addr)
		panic("No available memory found for relocation");

	return (ulong)dest_addr;
}

int dram_init_banksize(void)
{
	/* TODO: Set up memory banks properly*/
	gd->bd->bi_dram[0].start = 0x90000000;
	gd->bd->bi_dram[0].size = 0x1d000000;

	return 0;
}

/*
 * MRC scrambler seed offsets should be reserved in
 * mainboard cmos.layout and not covered by checksum.
 */
#if CONFIG_USE_OPTION_TABLE
#include "option_table.h"
#define CMOS_OFFSET_MRC_SEED     (CMOS_VSTART_mrc_scrambler_seed >> 3)
#define CMOS_OFFSET_MRC_SEED_S3  (CMOS_VSTART_mrc_scrambler_seed_s3 >> 3)
#define CMOS_OFFSET_MRC_SEED_CHK (CMOS_VSTART_mrc_scrambler_seed_chk >> 3)
#else
#define CMOS_OFFSET_MRC_SEED     152
#define CMOS_OFFSET_MRC_SEED_S3  156
#define CMOS_OFFSET_MRC_SEED_CHK 160
#endif

static void save_mrc_data(struct pei_data *pei_data)
{
	u16 c1, c2, checksum;

#if CONFIG_EARLY_CBMEM_INIT
	struct mrc_data_container *mrcdata;
	int output_len = ALIGN(pei_data->mrc_output_len, 16);

	/* Save the MRC S3 restore data to cbmem */
	cbmem_initialize();
	mrcdata = cbmem_add
		(CBMEM_ID_MRCDATA,
		 output_len + sizeof(struct mrc_data_container));

	debug("Relocate MRC DATA from %p to %p (%u bytes)\n",
	      pei_data->mrc_output, mrcdata, output_len);

	mrcdata->mrc_signature = MRC_DATA_SIGNATURE;
	mrcdata->mrc_data_size = output_len;
	mrcdata->reserved = 0;
	memcpy(mrcdata->mrc_data, pei_data->mrc_output,
	       pei_data->mrc_output_len);

	/* Zero the unused space in aligned buffer. */
	if (output_len > pei_data->mrc_output_len)
		memset(mrcdata->mrc_data+pei_data->mrc_output_len, 0,
		       output_len - pei_data->mrc_output_len);

	mrcdata->mrc_checksum = compute_ip_checksum(mrcdata->mrc_data,
						    mrcdata->mrc_data_size);
#endif

	/* Save the MRC seed values to CMOS */
	cmos_write32(CMOS_OFFSET_MRC_SEED, pei_data->scrambler_seed);
	debug("Save scrambler seed    0x%08x to CMOS 0x%02x\n",
	      pei_data->scrambler_seed, CMOS_OFFSET_MRC_SEED);

	cmos_write32(CMOS_OFFSET_MRC_SEED_S3, pei_data->scrambler_seed_s3);
	debug("Save s3 scrambler seed 0x%08x to CMOS 0x%02x\n",
	       pei_data->scrambler_seed_s3, CMOS_OFFSET_MRC_SEED_S3);

	/* Save a simple checksum of the seed values */
	c1 = compute_ip_checksum((u8*)&pei_data->scrambler_seed,
				 sizeof(u32));
	c2 = compute_ip_checksum((u8*)&pei_data->scrambler_seed_s3,
				 sizeof(u32));
	checksum = add_ip_checksums(sizeof(u32), c1, c2);

	cmos_write(checksum & 0xff, CMOS_OFFSET_MRC_SEED_CHK);
	cmos_write((checksum >> 8) & 0xff, CMOS_OFFSET_MRC_SEED_CHK+1);
}

static void prepare_mrc_cache(struct pei_data *pei_data)
{
	struct mrc_data_container *mrc_cache;
	u16 c1, c2, checksum, seed_checksum;

	// preset just in case there is an error
	pei_data->mrc_input = NULL;
	pei_data->mrc_input_len = 0;

	/* Read scrambler seeds from CMOS */
	pei_data->scrambler_seed = cmos_read32(CMOS_OFFSET_MRC_SEED);
	debug("Read scrambler seed    0x%08x from CMOS 0x%02x\n",
	      pei_data->scrambler_seed, CMOS_OFFSET_MRC_SEED);

	pei_data->scrambler_seed_s3 = cmos_read32(CMOS_OFFSET_MRC_SEED_S3);
	debug("Read S3 scrambler seed 0x%08x from CMOS 0x%02x\n",
	      pei_data->scrambler_seed_s3, CMOS_OFFSET_MRC_SEED_S3);

	/* Compute seed checksum and compare */
	c1 = compute_ip_checksum((u8*)&pei_data->scrambler_seed,
				 sizeof(u32));
	c2 = compute_ip_checksum((u8*)&pei_data->scrambler_seed_s3,
				 sizeof(u32));
	checksum = add_ip_checksums(sizeof(u32), c1, c2);

	seed_checksum = cmos_read(CMOS_OFFSET_MRC_SEED_CHK);
	seed_checksum |= cmos_read(CMOS_OFFSET_MRC_SEED_CHK+1) << 8;

	if (checksum != seed_checksum) {
		debug("%s: invalid seed checksum\n", __func__);
		pei_data->scrambler_seed = 0;
		pei_data->scrambler_seed_s3 = 0;
		return;
	}

	if ((mrc_cache = find_current_mrc_cache()) == NULL) {
		/* error message printed in find_current_mrc_cache */
		return;
	}

	pei_data->mrc_input = mrc_cache->mrc_data;
	pei_data->mrc_input_len = mrc_cache->mrc_data_size;

	debug("%s: at %p, size %x checksum %04x\n",
	      __func__, pei_data->mrc_input,
	      pei_data->mrc_input_len, mrc_cache->mrc_checksum);
}

static const char* ecc_decoder[] = {
	"inactive",
	"active on IO",
	"disabled on IO",
	"active"
};

/*
 * Dump in the log memory controller configuration as read from the memory
 * controller registers.
 */
static void report_memory_config(void)
{
	u32 addr_decoder_common, addr_decode_ch[2];
	int i;

	addr_decoder_common = MCHBAR32(0x5000);
	addr_decode_ch[0] = MCHBAR32(0x5004);
	addr_decode_ch[1] = MCHBAR32(0x5008);

	debug("memcfg DDR3 clock %d MHz\n",
	      (MCHBAR32(0x5e04) * 13333 * 2 + 50)/100);
	debug("memcfg channel assignment: A: %d, B % d, C % d\n",
	       addr_decoder_common & 3,
	       (addr_decoder_common >> 2) & 3,
	       (addr_decoder_common >> 4) & 3);

	for (i = 0; i < ARRAY_SIZE(addr_decode_ch); i++) {
		u32 ch_conf = addr_decode_ch[i];
		debug("memcfg channel[%d] config (%8.8x):\n", i, ch_conf);
		debug("   ECC %s\n", ecc_decoder[(ch_conf >> 24) & 3]);
		debug("   enhanced interleave mode %s\n",
		      ((ch_conf >> 22) & 1) ? "on" : "off");
		debug("   rank interleave %s\n",
		      ((ch_conf >> 21) & 1) ? "on" : "off");
		debug("   DIMMA %d MB width x%d %s rank%s\n",
		      ((ch_conf >> 0) & 0xff) * 256,
		      ((ch_conf >> 19) & 1) ? 16 : 8,
		      ((ch_conf >> 17) & 1) ? "dual" : "single",
		      ((ch_conf >> 16) & 1) ? "" : ", selected");
		debug("   DIMMB %d MB width x%d %s rank%s\n",
		      ((ch_conf >> 8) & 0xff) * 256,
		      ((ch_conf >> 20) & 1) ? 16 : 8,
		      ((ch_conf >> 18) & 1) ? "dual" : "single",
		      ((ch_conf >> 16) & 1) ? ", selected" : "");
	}
}

static void post_system_agent_init(struct pei_data *pei_data)
{
	/* If PCIe init is skipped, set the PEG clock gating */
	if (!pei_data->pcie_init)
		MCHBAR32(0x7010) = MCHBAR32(0x7010) | 0x01;
}

/* TODO: Fix up calling convention */
static void console_tx_byte(unsigned char byte)
{
#ifdef DEBUG
	ulong addr = cpu_get_sp() + 4;
	char ch = *(char *)addr;

	putc(ch);
#endif
}

/**
 * Find PEI executable in coreboot filesystem and execute it.
 *
 * @param pei_data: configuration data for UEFI PEI reference code
 */
int sdram_initialise(struct pei_data *pei_data)
{
	const char *data;
	uint16_t done;
	int ret;

	report_platform_info();

	/* Wait for ME to be ready */
	ret = intel_early_me_init();
	if (ret)
		return ret;
	ret = intel_early_me_uma_size();
	if (ret < 0)
		return ret;

	debug("Starting UEFI PEI System Agent\n");

	/*
	 * Do not pass MRC data in for recovery mode boot,
	 * Always pass it in for S3 resume.
	 */
// 	if (!recovery_mode_enabled() || pei_data->boot_mode == PEI_BOOT_RESUME)
	if (0)
		prepare_mrc_cache(pei_data);

	/* If MRC data is not found we cannot continue S3 resume. */
	if (pei_data->boot_mode == PEI_BOOT_RESUME && !pei_data->mrc_input) {
		debug("Giving up in sdram_initialize: No MRC data\n");
		outb(0x6, 0xcf9);
		hlt();
	}

	/* Pass console handler in pei_data */
	pei_data->tx_byte = console_tx_byte;

	debug("PEI data at %p, size %x:\n", pei_data, sizeof(*pei_data));

	data = (char *)0xfffa0000;
	if (data) {
		int rv;
		int (*func)(struct pei_data *);
		void setup_no_gdt(gd_t *id, u64 *gdt_addr);
		void setup_gdt(gd_t *id, u64 *gdt_addr);

		debug("Calling MRC at %p\n", data);
		post_code(0xb1);
		func = (int (*)(struct pei_data *))data;
		rv = func(pei_data);
		post_code(0xb3);
		if (rv) {
			switch (rv) {
			case -1:
				printf("PEI version mismatch.\n");
				break;
			case -2:
				printf("Invalid memory frequency.\n");
				break;
			default:
				printf("MRC returned %x.\n", rv);
			}
			printf("Nonzero MRC return value.\n");
			return -EFAULT;
		}
	} else {
		printf("UEFI PEI System Agent not found.\n");
		return -ENOSYS;
	}

#if CONFIG_USBDEBUG
	/* mrc.bin reconfigures USB, so reinit it to have debug */
	early_usbdebug_init();
#endif

	/* For reference print the System Agent version
	 * after executing the UEFI PEI stage.
	 */
	u32 version = MCHBAR32(0x5034);
	debug("System Agent Version %d.%d.%d Build %d\n",
		version >> 24 , (version >> 16) & 0xff,
		(version >> 8) & 0xff, version & 0xff);

	/* e for SandyBridge here.  This is done
	 * inside the SystemAgent binary on IvyBridge. */
	done = pci_read_config32(PCI_CPU_DEVICE, PCI_DEVICE_ID);
	done &= BASE_REV_MASK;
	if (BASE_REV_SNB == done)
		intel_early_me_init_done(ME_INIT_STATUS_SUCCESS);
	else
		intel_early_me_status();

	post_system_agent_init(pei_data);
	report_memory_config();

	/* S3 resume: don't save scrambler seed or MRC data */
	if (pei_data->boot_mode != PEI_BOOT_RESUME)
		save_mrc_data(pei_data);

	return 0;
}

static int copy_spd(struct pei_data *peid)
{
	const int gpio_vector[] = {41, 42, 43, 10, -1};
	int spd_index;
	const void *blob = gd->fdt_blob;
	int node, spd_node;
	int ret, i;

	debug("%s\n", __func__);
	for (i = 0; ; i++) {
		if (gpio_vector[i] == -1)
			break;
		ret = gpio_requestf(gpio_vector[i], "spd_id%d", i);
		if (ret) {
			debug("%s: Could not request gpio %d\n", __func__,
			      gpio_vector[i]);
			return ret;
		}
	}
	spd_index = gpio_get_values_as_int(gpio_vector);
	debug("spd index %d\n", spd_index);
	node = fdtdec_next_compatible(blob, 0, COMPAT_MEMORY_SPD);
	if (node < 0) {
		printf("SPD data not found.\n");
		return -ENOENT;
	}

	for (spd_node = fdt_first_subnode(blob, node);
	     spd_node > 0;
	     spd_node = fdt_next_subnode(blob, spd_node)) {
		const char *data;
		int len;

		if (fdtdec_get_int(blob, spd_node, "reg", -1) != spd_index)
			continue;
		data = fdt_getprop(blob, spd_node, "data", &len);
		if (len < sizeof(peid->spd_data[0])) {
			printf("Missing SPD data\n");
			return -EINVAL;
		}

		debug("Using SDRAM SPD data for '%s'\n",
		      fdt_get_name(blob, spd_node, NULL));
		memcpy(peid->spd_data[0], data, sizeof(peid->spd_data[0]));
		break;
	}

	if (spd_node < 0) {
		printf("No SPD data found for index %d\n", spd_index);
		return -ENOENT;
	}

	return 0;
}

static void rcba_config(void)
{
	u32 reg32;

	/*
	 *             GFX    INTA -> PIRQA (MSI)
	 * D28IP_P3IP  WLAN   INTA -> PIRQB
	 * D29IP_E1P   EHCI1  INTA -> PIRQD
	 * D26IP_E2P   EHCI2  INTA -> PIRQF
	 * D31IP_SIP   SATA   INTA -> PIRQF (MSI)
	 * D31IP_SMIP  SMBUS  INTB -> PIRQH
	 * D31IP_TTIP  THRT   INTC -> PIRQA
	 * D27IP_ZIP   HDA    INTA -> PIRQA (MSI)
	 *
	 * TRACKPAD                -> PIRQE (Edge Triggered)
	 * TOUCHSCREEN             -> PIRQG (Edge Triggered)
	 */

	/* Device interrupt pin register (board specific) */
	RCBA32(D31IP) = (INTC << D31IP_TTIP) | (NOINT << D31IP_SIP2) |
		(INTB << D31IP_SMIP) | (INTA << D31IP_SIP);
	RCBA32(D30IP) = (NOINT << D30IP_PIP);
	RCBA32(D29IP) = (INTA << D29IP_E1P);
	RCBA32(D28IP) = (INTA << D28IP_P3IP);
	RCBA32(D27IP) = (INTA << D27IP_ZIP);
	RCBA32(D26IP) = (INTA << D26IP_E2P);
	RCBA32(D25IP) = (NOINT << D25IP_LIP);
	RCBA32(D22IP) = (NOINT << D22IP_MEI1IP);

	/* Device interrupt route registers */
	DIR_ROUTE(D31IR, PIRQB, PIRQH, PIRQA, PIRQC);
	DIR_ROUTE(D29IR, PIRQD, PIRQE, PIRQF, PIRQG);
	DIR_ROUTE(D28IR, PIRQB, PIRQC, PIRQD, PIRQE);
	DIR_ROUTE(D27IR, PIRQA, PIRQH, PIRQA, PIRQB);
	DIR_ROUTE(D26IR, PIRQF, PIRQE, PIRQG, PIRQH);
	DIR_ROUTE(D25IR, PIRQA, PIRQB, PIRQC, PIRQD);
	DIR_ROUTE(D22IR, PIRQA, PIRQB, PIRQC, PIRQD);

	/* Enable IOAPIC (generic) */
	RCBA16(OIC) = 0x0100;
	/* PCH BWG says to read back the IOAPIC enable register */
	(void) RCBA16(OIC);

	/* Disable unused devices (board specific) */
	reg32 = RCBA32(FD);
	reg32 |= PCH_DISABLE_ALWAYS;
	RCBA32(FD) = reg32;
}

int dram_init(void)
{
	struct pei_data pei_data __attribute__ ((aligned (8))) = {
		pei_version: PEI_VERSION,
		mchbar: DEFAULT_MCHBAR,
		dmibar: DEFAULT_DMIBAR,
		epbar: DEFAULT_EPBAR,
		pciexbar: CONFIG_MMCONF_BASE_ADDRESS,
		smbusbar: SMBUS_IO_BASE,
		wdbbar: 0x4000000,
		wdbsize: 0x1000,
		hpet_address: CONFIG_HPET_ADDRESS,
		rcba: DEFAULT_RCBABASE,
		pmbase: DEFAULT_PMBASE,
		gpiobase: DEFAULT_GPIOBASE,
		thermalbase: 0xfed08000,
		system_type: 0, // 0 Mobile, 1 Desktop/Server
		tseg_size: CONFIG_SMM_TSEG_SIZE,
		ts_addresses: { 0x00, 0x00, 0x00, 0x00 },
		ec_present: 1,
		ddr3lv_support: 1,
		// 0 = leave channel enabled
		// 1 = disable dimm 0 on channel
		// 2 = disable dimm 1 on channel
		// 3 = disable dimm 0+1 on channel
		dimm_channel0_disabled: 2,
		dimm_channel1_disabled: 2,
		max_ddr3_freq: 1600,
		usb_port_config: {
			/* Empty and onboard Ports 0-7, set to un-used pin OC3 */
			{ 0, 3, 0x0000 }, /* P0: Empty */
			{ 1, 0, 0x0040 }, /* P1: Left USB 1  (OC0) */
			{ 1, 1, 0x0040 }, /* P2: Left USB 2  (OC1) */
			{ 1, 3, 0x0040 }, /* P3: SDCARD      (no OC) */
			{ 0, 3, 0x0000 }, /* P4: Empty */
			{ 1, 3, 0x0040 }, /* P5: WWAN        (no OC) */
			{ 0, 3, 0x0000 }, /* P6: Empty */
			{ 0, 3, 0x0000 }, /* P7: Empty */
			/* Empty and onboard Ports 8-13, set to un-used pin OC4 */
			{ 1, 4, 0x0040 }, /* P8: Camera      (no OC) */
			{ 1, 4, 0x0040 }, /* P9: Bluetooth   (no OC) */
			{ 0, 4, 0x0000 }, /* P10: Empty */
			{ 0, 4, 0x0000 }, /* P11: Empty */
			{ 0, 4, 0x0000 }, /* P12: Empty */
			{ 0, 4, 0x0000 }, /* P13: Empty */
		},
	};
	ulong top_of_memory;
	int ret;

	debug("Boot mode %d\n", gd->arch.pei_boot_mode);
	debug("mcr_input %p\n", pei_data.mrc_input);
	pei_data.boot_mode = gd->arch.pei_boot_mode;
	ret = copy_spd(&pei_data);
	if (!ret)
		ret = sdram_initialise(&pei_data);
	if (ret)
		return ret;

	post_code(0x3c);

	rcba_config();
	post_code(0x3d);

	quick_ram_check();
	post_code(0x3e);

	MCHBAR16(SSKPD) = 0xCAFE;

#if CONFIG_HAVE_ACPI_RESUME
	/* If there is no high memory area, we didn't boot before, so
	 * this is not a resume. In that case we just create the cbmem toc.
	 */

	*(u32 *)CBMEM_BOOT_MODE = 0;
	*(u32 *)CBMEM_RESUME_BACKUP = 0;

	if ((boot_mode == 2) && cbmem_was_initted) {
		void *resume_backup_memory = cbmem_find(CBMEM_ID_RESUME);
		if (resume_backup_memory) {
			*(u32 *)CBMEM_BOOT_MODE = boot_mode;
			*(u32 *)CBMEM_RESUME_BACKUP = (u32)resume_backup_memory;
		}
		/* Magic for S3 resume */
		pci_write_config32(PCI_BDF_CB(0, 0x00, 0), SKPAD, 0xcafed00d);
	} else if (boot_mode == 2) {
		/* Failed S3 resume, reset to come up cleanly */
		outb(0x6, 0xcf9);
		hlt();
	} else {
		pci_write_config32(PCI_BDF_CB(0, 0x00, 0), SKPAD, 0xcafebabe);
	}
#endif
	post_code(0x3f);
#if CONFIG_CHROMEOS
	init_chromeos(boot_mode);
#endif

	/* Base of TSEG is top of usable DRAM */
	top_of_memory = pci_read_config32(PCI_BDF_CB(0,0,0), TSEG);

	/* TODO: Set up memory size properly*/
	debug("RAM top %lx\n", top_of_memory);
	gd->ram_size = top_of_memory - 0x90000000;

	return dram_init_banksize();
}
