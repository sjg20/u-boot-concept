/*
 * (C) Copyright 2002
 * Daniel Engström, Omicron Ceti AB, daniel@omicron.se
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __ASM_PROCESSOR_H_
#define __ASM_PROCESSOR_H_ 1

#define X86_GDT_ENTRY_SIZE	8

#define ROM_CODE_SEG 0x10
#define ROM_DATA_SEG 0x18

#ifndef __ASSEMBLY__

enum {
	X86_GDT_ENTRY_NULL = 0,
	X86_GDT_ENTRY_32BIT_CS,
	X86_GDT_ENTRY_32BIT_DS,
	X86_GDT_ENTRY_32BIT_FS,
	X86_GDT_ENTRY_16BIT_CS,
	X86_GDT_ENTRY_16BIT_DS,
	X86_GDT_NUM_ENTRIES
};
#else
/* NOTE: If the above enum is modified, this define must be checked */
#define X86_GDT_ENTRY_32BIT_DS	2
#define X86_GDT_NUM_ENTRIES	6
#endif

#define X86_GDT_SIZE		(X86_GDT_NUM_ENTRIES * X86_GDT_ENTRY_SIZE)

#ifndef __ASSEMBLY__
static inline __attribute__((always_inline)) void hlt(void)
{
	asm("hlt");
}

struct cpuid_result {
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
};

/*
 * Generic CPUID function
 */
static inline struct cpuid_result cpuid(int op)
{
	struct cpuid_result result;
	asm volatile(
		"mov %%ebx, %%edi;"
		"cpuid;"
		"mov %%ebx, %%esi;"
		"mov %%edi, %%ebx;"
		: "=a" (result.eax),
		  "=S" (result.ebx),
		  "=c" (result.ecx),
		  "=d" (result.edx)
		: "0" (op)
		: "edi");
	return result;
}

static inline ulong cpu_get_sp(void)
{
	ulong result;

	asm volatile(
		"mov %%esp, %%eax"
		: "=a" (result));
	return result;
}

#endif

#endif
