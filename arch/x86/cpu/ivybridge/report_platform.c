/*
 * Copied from Coreboot src/northbridge/intel/sandybridge/report_platform.c
 *
 * Copyright (C) 2012 Google Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */
#define DEBUG

#include <common.h>
#include <asm/processor.h>
#include <asm/arch/pch.h>

static void report_cpu_info(void)
{
	struct cpuid_result cpuidr;
	u32 i, index;
	char cpu_string[50], *cpu_name = cpu_string; /* 48 bytes are reported */
	int vt, txt, aes;
	const char *mode[] = {"NOT ", ""};

	index = 0x80000000;
	cpuidr = cpuid(index);
	if (cpuidr.eax < 0x80000004) {
		strcpy(cpu_string, "Platform info not available");
	} else {
		u32 *p = (u32*) cpu_string;
		for (i = 2; i <= 4 ; i++) {
			cpuidr = cpuid(index + i);
			*p++ = cpuidr.eax;
			*p++ = cpuidr.ebx;
			*p++ = cpuidr.ecx;
			*p++ = cpuidr.edx;
		}
	}
	/* Skip leading spaces in CPU name string */
	while (cpu_name[0] == ' ')
		cpu_name++;

	cpuidr = cpuid(1);
	debug("CPU id(%x): %s\n", cpuidr.eax, cpu_name);
	aes = (cpuidr.ecx & (1 << 25)) ? 1 : 0;
	txt = (cpuidr.ecx & (1 << 6)) ? 1 : 0;
	vt = (cpuidr.ecx & (1 << 5)) ? 1 : 0;
	debug("AES %ssupported, TXT %ssupported, VT %ssupported\n",
	       mode[aes], mode[txt], mode[vt]);
}

/* The PCI id name match comes from Intel document 472178 */
static struct {
	u16 dev_id;
	const char *dev_name;
} pch_table [] = {
	{0x1E41, "Desktop Sample"},
	{0x1E42, "Mobile Sample"},
	{0x1E43, "SFF Sample"},
	{0x1E44, "Z77"},
	{0x1E45, "H71"},
	{0x1E46, "Z75"},
	{0x1E47, "Q77"},
	{0x1E48, "Q75"},
	{0x1E49, "B75"},
	{0x1E4A, "H77"},
	{0x1E53, "C216"},
	{0x1E55, "QM77"},
	{0x1E56, "QS77"},
	{0x1E58, "UM77"},
	{0x1E57, "HM77"},
	{0x1E59, "HM76"},
	{0x1E5D, "HM75"},
	{0x1E5E, "HM70"},
	{0x1E5F, "NM70"},
};

static void report_pch_info(void)
{
	int i;
	u16 dev_id;
	uint8_t rev_id;

	dev_id = pci_read_config16(PCH_LPC_DEV, 2);

	const char *pch_type = "Unknown";
	for (i = 0; i < ARRAY_SIZE(pch_table); i++) {
		if (pch_table[i].dev_id == dev_id) {
			pch_type = pch_table[i].dev_name;
			break;
		}
	}
	rev_id = pci_read_config8(PCH_LPC_DEV, 8);
	debug("PCH type: %s, device id: %x, rev id %x\n", pch_type, dev_id,
	      rev_id);
}

void report_platform_info(void)
{
	report_cpu_info();
	report_pch_info();
}
