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
#include <malloc.h>
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
	struct memory_info *info = &gd->arch.meminfo;
	uintptr_t dest_addr = 0;
	struct memory_area *largest = NULL;
	int i;

	/* Find largest area of memory below 4GB */

	for (i = 0; i < info->num_areas; i++) {
		struct memory_area *area = &info->area[i];

		if (area->start >= 1ULL << 32)
			continue;
		if (!largest || area->size > largest->size)
			largest = area;
	}

	/* If no suitable area was found, return an error. */
	assert(largest);
	if (!largest || largest->size < (2 << 20))
		panic("No available memory found for relocation");

	dest_addr = largest->start + largest->size;

	return (ulong)dest_addr;
}

void dram_init_banksize(void)
{
	struct memory_info *info = &gd->arch.meminfo;
	int num_banks;
	int i;

	for (i = 0, num_banks = 0; i < info->num_areas; i++) {
		struct memory_area *area = &info->area[i];

		if (area->start >= 1ULL << 32)
			continue;
		gd->bd->bi_dram[num_banks].start = area->start;
		gd->bd->bi_dram[num_banks].size = area->size;
		num_banks++;
	}
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

static int recovery_mode_enabled(void)
{
	return false;
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
	if (!recovery_mode_enabled() || pei_data->boot_mode == PEI_BOOT_RESUME)
		prepare_mrc_cache(pei_data);

	/* If MRC data is not found we cannot continue S3 resume. */
	if (pei_data->boot_mode == PEI_BOOT_RESUME && !pei_data->mrc_input) {
		debug("Giving up in sdram_initialize: No MRC data\n");
		outb(0x6, 0xcf9);
		cpu_hlt();
	}

	/* Pass console handler in pei_data */
	pei_data->tx_byte = console_tx_byte;

	debug("PEI data at %p, size %x:\n", pei_data, sizeof(*pei_data));

	data = (char *)0xfffa0000;
	if (data) {
		int rv;
		int (*func)(struct pei_data *);

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

/**
 * add_memory_area() - Add a new usable memory area to our list
 *
 * Note: @start and @end must not span the first 4GB boundary
 *
 * @info:	Place to store memory info
 * @start:	Start of this memory area
 * @end:	End of this memory area + 1
 */
static int add_memory_area(struct memory_info *info,
			   uint64_t start, uint64_t end)
{
	struct memory_area *ptr;

	if (info->num_areas == CONFIG_NR_DRAM_BANKS)
		return -ENOSPC;

	ptr = &info->area[info->num_areas];
	ptr->start = start;
	ptr->size = end - start;
	info->total_memory += ptr->size;
	if (ptr->start < (1ULL << 32))
		info->total_32bit_memory += ptr->size;
	debug("%d: memory %llx size %llx, total now %llx / %llx\n",
	      info->num_areas, ptr->start, ptr->size,
	      info->total_32bit_memory, info->total_memory);
	info->num_areas++;

	return 0;
}

/**
 * sdram_find() - Find available memory
 *
 * This is a bit complicated since on x86 there are system memory holes all
 * over the place. We create a list of available memory blocks
 */
static int sdram_find(pci_dev_t dev)
{
	uint64_t tom, me_base, touud;
	uint32_t tseg_base, uma_size, tolud;
	uint16_t ggc;
	unsigned long long tomk;
	/* IGD UMA memory */
	uint64_t uma_memory_base = 0;
	uint64_t uma_memory_size;
	struct memory_info *info = &gd->arch.meminfo;

	/* Total Memory 2GB example:
	 *
	 *  00000000  0000MB-1992MB  1992MB  RAM     (writeback)
	 *  7c800000  1992MB-2000MB     8MB  TSEG    (SMRR)
	 *  7d000000  2000MB-2002MB     2MB  GFX GTT (uncached)
	 *  7d200000  2002MB-2034MB    32MB  GFX UMA (uncached)
	 *  7f200000   2034MB TOLUD
	 *  7f800000   2040MB MEBASE
	 *  7f800000  2040MB-2048MB     8MB  ME UMA  (uncached)
	 *  80000000   2048MB TOM
	 * 100000000  4096MB-4102MB     6MB  RAM     (writeback)
	 *
	 * Total Memory 4GB example:
	 *
	 *  00000000  0000MB-2768MB  2768MB  RAM     (writeback)
	 *  ad000000  2768MB-2776MB     8MB  TSEG    (SMRR)
	 *  ad800000  2776MB-2778MB     2MB  GFX GTT (uncached)
	 *  ada00000  2778MB-2810MB    32MB  GFX UMA (uncached)
	 *  afa00000   2810MB TOLUD
	 *  ff800000   4088MB MEBASE
	 *  ff800000  4088MB-4096MB     8MB  ME UMA  (uncached)
	 * 100000000   4096MB TOM
	 * 100000000  4096MB-5374MB  1278MB  RAM     (writeback)
	 * 14fe00000   5368MB TOUUD
	 */

	/* Top of Upper Usable DRAM, including remap */
	touud = pci_read_config32(dev, TOUUD+4);
	touud <<= 32;
	touud |= pci_read_config32(dev, TOUUD);

	/* Top of Lower Usable DRAM */
	tolud = pci_read_config32(dev, TOLUD);

	/* Top of Memory - does not account for any UMA */
	tom = pci_read_config32(dev, 0xa4);
	tom <<= 32;
	tom |= pci_read_config32(dev, 0xa0);

	debug("TOUUD %llx TOLUD %08x TOM %llx\n", touud, tolud, tom);

	/* ME UMA needs excluding if total memory <4GB */
	me_base = pci_read_config32(dev, 0x74);
	me_base <<= 32;
	me_base |= pci_read_config32(dev, 0x70);

	debug("MEBASE %llx\n", me_base);

	// TODO: Get rid of all this shifting by 10 bits
	tomk = tolud >> 10;
	if (me_base == tolud) {
		/* ME is from MEBASE-TOM */
		uma_size = (tom - me_base) >> 10;
		/* Increment TOLUD to account for ME as RAM */
		tolud += uma_size << 10;
		/* UMA starts at old TOLUD */
		uma_memory_base = tomk * 1024ULL;
		uma_memory_size = uma_size * 1024ULL;
		debug("ME UMA base %llx size %uM\n", me_base, uma_size >> 10);
	}

	/* Graphics memory comes next */
	ggc = pci_read_config16(dev, GGC);
	if (!(ggc & 2)) {
		debug("IGD decoded, subtracting ");

		/* Graphics memory */
		uma_size = ((ggc >> 3) & 0x1f) * 32 * 1024ULL;
		debug("%uM UMA", uma_size >> 10);
		tomk -= uma_size;
		uma_memory_base = tomk * 1024ULL;
		uma_memory_size += uma_size * 1024ULL;

		/* GTT Graphics Stolen Memory Size (GGMS) */
		uma_size = ((ggc >> 8) & 0x3) * 1024ULL;
		tomk -= uma_size;
		uma_memory_base = tomk * 1024ULL;
		uma_memory_size += uma_size * 1024ULL;
		debug(" and %uM GTT\n", uma_size >> 10);
	}

	/* Calculate TSEG size from its base which must be below GTT */
	tseg_base = pci_read_config32(dev, 0xb8);
	uma_size = (uma_memory_base - tseg_base) >> 10;
	tomk -= uma_size;
	uma_memory_base = tomk * 1024ULL;
	uma_memory_size += uma_size * 1024ULL;
	debug("TSEG base 0x%08x size %uM\n",
	       tseg_base, uma_size >> 10);

	debug("Available memory below 4GB: %lluM\n", tomk >> 10);

	/* Report the memory regions */
	add_memory_area(info, 1 << 20, 2 << 28);
	add_memory_area(info, (2 << 28) + (2 << 20), 4 << 28);
	add_memory_area(info, (4 << 28) + (2 << 20), tseg_base);
	add_memory_area(info, 1ULL << 32, touud);
/*
	ram_resource(dev, 3, 0, legacy_hole_base_k);
	ram_resource(dev, 4, legacy_hole_base_k + legacy_hole_size_k,
	     (tomk - (legacy_hole_base_k + legacy_hole_size_k)));
*/
	/*
	 * If >= 4GB installed then memory from TOLUD to 4GB
	 * is remapped above TOM, TOUUD will account for both
	 */
	if (touud > (1ULL << 32ULL)) {
// 		ram_resource(dev, 5, 4096 * 1024, touud - (4096 * 1024));
		debug("Available memory above 4GB: %lluM\n",
		       (touud >> 20) - 4096);
	}
#if 0
	assign_resources(dev->link_list);

	/* Leave some space for ACPI, PIRQ and MP tables */
	high_tables_base = (tomk * 1024) - HIGH_MEMORY_SIZE;
	high_tables_size = HIGH_MEMORY_SIZE;
#endif

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
	pci_dev_t dev = PCI_BDF_CB(0,0,0);
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

	/* TODO: Work out ACPI resume */
#if CONFIG_HAVE_ACPI_RESUME && 0
	/*
	 * If there is no high memory area, we didn't boot before, so
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
		cpu_hlt();
	} else {
		pci_write_config32(PCI_BDF_CB(0, 0x00, 0), SKPAD, 0xcafebabe);
	}
#endif
	post_code(0x3f);
#if CONFIG_CHROMEOS
	init_chromeos(boot_mode);
#endif

	ret = sdram_find(dev);
	if (ret)
		return ret;

	gd->ram_size = gd->arch.meminfo.total_32bit_memory;

	return 0;
}
