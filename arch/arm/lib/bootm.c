// SPDX-License-Identifier: GPL-2.0+
/* Copyright (C) 2011
 * Corscience GmbH & Co. KG - Simon Schwarz <schwarz@corscience.de>
 *  - Added prep subcommand support
 *  - Reorganized source - modeled after powerpc version
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * Copyright (C) 2001  Erik Mouw (J.A.K.Mouw@its.tudelft.nl)
 */

#define LOG_CATEGORY	LOGC_BOOT

#include <bootm.h>
#include <bootstage.h>
#include <command.h>
#include <cpu_func.h>
#include <display_options.h>
#include <dm.h>
#include <fs_legacy.h>
#include <init.h>
#include <log.h>
#include <asm/global_data.h>
#include <dm/root.h>
#include <env.h>
#include <image.h>
#include <u-boot/zlib.h>
#include <asm/byteorder.h>
#include <linux/libfdt.h>
#include <mapmem.h>
#include <fdt_support.h>
#include <asm/bootm.h>
#include <asm/secure.h>
#include <linux/compiler.h>
#include <bootm.h>
#include <vxworks.h>
#include <asm/cache.h>

#ifdef CONFIG_ARMV7_NONSEC
#include <asm/armv7.h>
#endif
#include <asm/setup.h>

DECLARE_GLOBAL_DATA_PTR;

static struct tag *params;

__weak void board_quiesce_devices(void)
{
}

static void setup_start_tag (struct bd_info *bd)
{
	params = (struct tag *)bd->bi_boot_params;

	params->hdr.tag = ATAG_CORE;
	params->hdr.size = tag_size (tag_core);

	params->u.core.flags = 0;
	params->u.core.pagesize = 0;
	params->u.core.rootdev = 0;

	params = tag_next (params);
}

static void setup_memory_tags(struct bd_info *bd)
{
	int i;

	for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++) {
		params->hdr.tag = ATAG_MEM;
		params->hdr.size = tag_size (tag_mem32);

		params->u.mem.start = bd->bi_dram[i].start;
		params->u.mem.size = bd->bi_dram[i].size;

		params = tag_next (params);
	}
}

static void setup_commandline_tag(struct bd_info *bd, char *commandline)
{
	char *p;

	if (!commandline)
		return;

	/* eat leading white space */
	for (p = commandline; *p == ' '; p++);

	/* skip non-existent command lines so the kernel will still
	 * use its default command line.
	 */
	if (*p == '\0')
		return;

	params->hdr.tag = ATAG_CMDLINE;
	params->hdr.size =
		(sizeof (struct tag_header) + strlen (p) + 1 + 4) >> 2;

	strcpy (params->u.cmdline.cmdline, p);

	params = tag_next (params);
}

static void setup_initrd_tag(struct bd_info *bd, ulong initrd_start,
			     ulong initrd_end)
{
	/* an ATAG_INITRD node tells the kernel where the compressed
	 * ramdisk can be found. ATAG_RDIMG is a better name, actually.
	 */
	params->hdr.tag = ATAG_INITRD2;
	params->hdr.size = tag_size (tag_initrd);

	params->u.initrd.start = initrd_start;
	params->u.initrd.size = initrd_end - initrd_start;

	params = tag_next (params);
}

static void setup_serial_tag(struct tag **tmp)
{
	struct tag *params = *tmp;
	struct tag_serialnr serialnr;

	get_board_serial(&serialnr);
	params->hdr.tag = ATAG_SERIAL;
	params->hdr.size = tag_size (tag_serialnr);
	params->u.serialnr.low = serialnr.low;
	params->u.serialnr.high= serialnr.high;
	params = tag_next (params);
	*tmp = params;
}

static void setup_revision_tag(struct tag **in_params)
{
	u32 rev = 0;

	rev = get_board_rev();
	params->hdr.tag = ATAG_REVISION;
	params->hdr.size = tag_size (tag_revision);
	params->u.revision.rev = rev;
	params = tag_next (params);
}

static void setup_end_tag(struct bd_info *bd)
{
	params->hdr.tag = ATAG_NONE;
	params->hdr.size = 0;
}

__weak void setup_board_tags(struct tag **in_params) {}

#ifdef CONFIG_ARM64
static void do_nonsec_virt_switch(void)
{
	if (ll_boot_init()) {
		smp_kick_all_cpus();
		dcache_disable();	/* flush cache before switching to EL2 */
	}
}
#endif

__weak void board_prep_linux(struct bootm_headers *images) { }

/* Subcommand: PREP */
static void boot_prep_linux(struct bootm_headers *images)
{
	char *commandline = env_get("bootargs");

	if (CONFIG_IS_ENABLED(OF_LIBFDT) && IS_ENABLED(CONFIG_LMB) && images->ft_len) {
		debug("using: FDT\n");
		if (image_setup_linux(images)) {
			panic("FDT creation failed!");
		}
	} else if (BOOTM_ENABLE_TAGS) {
		debug("using: ATAGS\n");
		setup_start_tag(gd->bd);
		if (BOOTM_ENABLE_SERIAL_TAG)
			setup_serial_tag(&params);
		if (BOOTM_ENABLE_CMDLINE_TAG)
			setup_commandline_tag(gd->bd, commandline);
		if (BOOTM_ENABLE_REVISION_TAG)
			setup_revision_tag(&params);
		if (BOOTM_ENABLE_MEMORY_TAGS)
			setup_memory_tags(gd->bd);
		if (BOOTM_ENABLE_INITRD_TAG) {
			/*
			 * In boot_ramdisk_high(), it may relocate ramdisk to
			 * a specified location. And set images->initrd_start &
			 * images->initrd_end to relocated ramdisk's start/end
			 * addresses. So use them instead of images->rd_start &
			 * images->rd_end when possible.
			 */
			if (images->initrd_start && images->initrd_end) {
				setup_initrd_tag(gd->bd, images->initrd_start,
						 images->initrd_end);
			} else if (images->rd_start && images->rd_end) {
				setup_initrd_tag(gd->bd, images->rd_start,
						 images->rd_end);
			}
		}
		setup_board_tags(&params);
		setup_end_tag(gd->bd);
	} else {
		panic("FDT and ATAGS support not compiled in\n");
	}

	board_prep_linux(images);
}

__weak bool armv7_boot_nonsec_default(void)
{
#ifdef CONFIG_ARMV7_BOOT_SEC_DEFAULT
	return false;
#else
	return true;
#endif
}

#ifdef CONFIG_ARMV7_NONSEC
bool armv7_boot_nonsec(void)
{
	char *s = env_get("bootm_boot_mode");
	bool nonsec = armv7_boot_nonsec_default();

	if (s && !strcmp(s, "sec"))
		nonsec = false;

	if (s && !strcmp(s, "nonsec"))
		nonsec = true;

	return nonsec;
}
#endif

#ifdef CONFIG_ARM64
__weak void update_os_arch_secondary_cores(uint8_t os_arch)
{
}

#if 0
#ifdef CONFIG_ARMV8_SWITCH_TO_EL1
static void switch_to_el1(void)
{
	if ((IH_ARCH_DEFAULT == IH_ARCH_ARM64) &&
	    (images.os.arch == IH_ARCH_ARM)) {
		printf("%d  ", __LINE__);
		armv8_switch_to_el1(0, (u64)gd->bd->bi_arch_number,
				    (u64)images.ft_addr, 0,
				    (u64)images.ep,
				    ES_TO_AARCH32);
	} else {
		printf("%d  ", __LINE__);
		armv8_switch_to_el1((u64)images.ft_addr, 0, 0, 0,
				    images.ep,
				    ES_TO_AARCH64);
	}
	printf("%d  ", __LINE__);
}
#endif
#endif
#endif

static void show_dt(void *fdt)
{
	int chosen;
	// int size;
	int node;

	printf("fdt %p\n", fdt);
	chosen = fdt_subnode_offset(fdt, 0, "chosen");
	printf("chosen %d\n", chosen);
	// printf("bootargs: %s\n",
	       // (char *)fdt_getprop(fdt, chosen, "bootargs", &size));
	printf("deleting initrd\n");
	fdt_delprop(fdt, chosen, "linux,initrd-start");
	fdt_delprop(fdt, chosen, "linux,initrd-end");
	fdt_print(fdt, chosen, 3);
#if 0
	// fdt_for_each_subnode(node, fdt, 0) {
	for (node = 0; node >= 0;
	     node = fdt_next_node(fdt, node, NULL)) {
		const char *name = fdt_get_name(fdt, node, NULL);

		// printf("- %s\n", name);
		// if (!strstr(name, "memory"))
			// continue;
		// if (!strstr(name, "framebuffer"))
			// continue;
		// if (!strstr(name, "watchdog"))
			// continue;
		if (strncmp("memory", name, 6) &&
		    strncmp("reserved-memory", name, 15))
			continue;
		// fdt_setprop(fdt, node, "status", "disabled", 9);

		fdt_print(fdt, node, 4);
	}
#endif
	//node = fdt_subnode_offset(fdt, 0, "watchdog@1c840000");
	// node = fdt_subnode_offset(fdt, 0, "watchdog");
	// printf("watchdog %d\n", node);
}

/* Subcommand: GO */
static void boot_jump_linux(struct bootm_headers *images, int flag)
{
#ifdef CONFIG_ARM64
	int fake = (flag & BOOTM_STATE_OS_FAKE_GO);
#if 0
	loff_t actread;
	char *dev, *other, *use_dev;
	char dev_part[10], *fname;
	int ret, part;
#endif
	printf("## Transferring control to Linux (at address %lx)...\n",
	       images->ep);
	bootstage_mark(BOOTSTAGE_ID_RUN_OS);
	print_buffer(images->ep, (void *)images->ep, 1, 64, 0);
#if 0
	dev = env_get("dev");
	other = !strcmp(dev, "1") ? "2" : "1";

	bool boot_same = false;

	if (boot_same) {
		use_dev = other;
		part = 1;
		fname = "orig";
	} else {
		use_dev = other;
		part = 1;
		fname = "Image";
	}
	sprintf(dev_part, "%s:%d", use_dev, part);
	if (fs_set_blk_dev("efi", dev_part, FS_TYPE_ANY)) {
		printf("2fs_set_blk_dev failed\n");
	}
	printf("Replacing kernel fname '%s' dev_part %s at %lx size %lx\n",
	       fname, dev_part, images->os.load, images->os.image_len);
	ret = fs_legacy_read(fname, images->os.load, 0, 0, &actread);
	printf("1ret = %d\n", ret);
#if 0
	if (ret) {
		if (fs_set_blk_dev("efi", "1", FS_TYPE_ANY)) {
			printf("1fs_set_blk_dev failed\n");
		}
		ret = fs_legacy_read("Image", images->os.load, 0, 0, &actread);
		printf("2ret = %d actread %lx end %lx\n", ret, (ulong)actread,
		       images->os.load + (ulong)actread);
	}
#endif
	print_buffer(images->ep, (void *)images->ep, 1, 64, 0);
#endif

	show_dt(images->ft_addr);
	// printf("booti %lx - %lx\n", images->os.load, (ulong)working_fdt);
	// printf("not booting\n");
	// return;
	bootm_final(fake ? BOOTM_FINAL_FAKE : 0);
	printf("still here\n");

	if (!fake) {
		printf("%d  ", __LINE__);
#ifdef CONFIG_ARMV8_PSCI
		printf("%d  ", __LINE__);
		armv8_setup_psci();
		printf("%d  ", __LINE__);
#endif
		printf("%d  ", __LINE__);
		do_nonsec_virt_switch();
		printf("%d  ", __LINE__);
//try these
		update_os_arch_secondary_cores(images->os.arch);
		printf("%d  ", __LINE__);

#ifdef CONFIG_ARMV8_SWITCH_TO_EL1
		printf("%d  ", __LINE__);
		// armv8_switch_to_el2((u64)images->ft_addr, 0, 0, 0,
				    // (u64)switch_to_el1, ES_TO_AARCH64);
		typedef void (*h_func)(ulong fdt, int zero, int arch,
				       uint params);
		h_func func = (h_func)images->ep;

		func((ulong)images->ft_addr, 0, 0, 0);

				    // (u64)switch_to_el1, ES_TO_AARCH64);

		// armv8_switch_to_el2((u64)images->ft_addr, 0, 0, 0,
				    // (u64)switch_to_el1, ES_TO_AARCH64);
		printf("%d  ", __LINE__);
#else
		printf("%d  ", __LINE__);
		if ((IH_ARCH_DEFAULT == IH_ARCH_ARM64) &&
		    (images->os.arch == IH_ARCH_ARM)) {
		printf("%d  ", __LINE__);
			armv8_switch_to_el2(0, (u64)gd->bd->bi_arch_number,
					    (u64)images->ft_addr, 0,
					    (u64)images->ep,
					    ES_TO_AARCH32);
		printf("%d  ", __LINE__);
		} else {
		printf("%d  ", __LINE__);  // gets to here
			armv8_switch_to_el2((u64)images->ft_addr, 0, 0, 0,
					    images->ep,
					    ES_TO_AARCH64);
		printf("%d  ", __LINE__);
		}
#endif
	}
#else
	unsigned long machid = gd->bd->bi_arch_number;
	char *s;
	void (*kernel_entry)(int zero, int arch, uint params);
	unsigned long r2;
	int fake = (flag & BOOTM_STATE_OS_FAKE_GO);

	kernel_entry = (void (*)(int, int, uint))images->ep;
#ifdef CONFIG_CPU_V7M
	ulong addr = (ulong)kernel_entry | 1;
	kernel_entry = (void *)addr;
#endif
	s = env_get("machid");
	if (s) {
		if (strict_strtoul(s, 16, &machid) < 0) {
			debug("strict_strtoul failed!\n");
			return;
		}
		printf("Using machid 0x%lx from environment\n", machid);
	}

	debug("## Transferring control to Linux (at address %08lx)" \
		"...\n", (ulong) kernel_entry);
	bootstage_mark(BOOTSTAGE_ID_RUN_OS);
	bootm_final(fake ? BOOTM_FINAL_FAKE : 0);

	if (CONFIG_IS_ENABLED(OF_LIBFDT) && images->ft_len)
		r2 = (unsigned long)images->ft_addr;
	else
		r2 = gd->bd->bi_boot_params;

	if (!fake) {
#ifdef CONFIG_ARMV7_NONSEC
		if (armv7_boot_nonsec()) {
			armv7_init_nonsec();
			secure_ram_addr(_do_nonsec_entry)(kernel_entry,
							  0, machid, r2);
		} else
#endif
			kernel_entry(0, machid, r2);
	}
#endif
		printf("%d  ", __LINE__);
}

/* Main Entry point for arm bootm implementation
 *
 * Modeled after the powerpc implementation
 * DIFFERENCE: Instead of calling prep and go at the end
 * they are called if subcommand is equal 0.
 */
int do_bootm_linux(int flag, struct bootm_info *bmi)
{
	struct bootm_headers *images = bmi->images;

	log_debug("boot linux flag %x\n", flag);
	/* No need for those on ARM */
	if (flag & BOOTM_STATE_OS_BD_T || flag & BOOTM_STATE_OS_CMDLINE)
		return -1;

	if (flag & BOOTM_STATE_OS_PREP) {
		log_debug("Preparing to boot Linux\n");
		boot_prep_linux(images);
		return 0;
	}

	if (flag & (BOOTM_STATE_OS_GO | BOOTM_STATE_OS_FAKE_GO)) {
		log_debug("Jumping to Linux (or faking it)\n");
		boot_jump_linux(images, flag);
		return 0;
	}

	log_debug("No flags set: continuing to prepare and jump to Linux\n");
	boot_prep_linux(images);
	boot_jump_linux(images, flag);
	return 0;
}

#if defined(CONFIG_BOOTM_VXWORKS)
void boot_prep_vxworks(struct bootm_headers *images)
{
#if defined(CONFIG_OF_LIBFDT)
	int off;

	if (images->ft_addr) {
		off = fdt_path_offset(images->ft_addr, "/memory");
		if (off > 0) {
			if (arch_fixup_fdt(images->ft_addr))
				puts("## WARNING: fixup memory failed!\n");
		}
	}
#endif
	cleanup_before_linux();
}

void boot_jump_vxworks(struct bootm_headers *images)
{
#if defined(CONFIG_ARM64) && defined(CONFIG_ARMV8_PSCI)
	armv8_setup_psci();
	smp_kick_all_cpus();
#endif

	/* ARM VxWorks requires device tree physical address to be passed */
	((void (*)(void *))images->ep)(images->ft_addr);
}
#endif
