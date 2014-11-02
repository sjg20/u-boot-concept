/*
 * (C) Copyright 2002
 * Daniel Engström, Omicron Ceti AB, daniel@omicron.se
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __ASM_PROCESSOR_H_
#define __ASM_PROCESSOR_H_ 1

#define X86_GDT_ENTRY_SIZE	8

#ifndef __ASSEMBLY__

enum {
	X86_GDT_ENTRY_NULL = 0,
	X86_GDT_ENTRY_UNUSED,
	X86_GDT_ENTRY_32BIT_CS,
	X86_GDT_ENTRY_32BIT_DS,
	X86_GDT_ENTRY_32BIT_FS,
	X86_GDT_ENTRY_16BIT_CS,
	X86_GDT_ENTRY_16BIT_DS,
	X86_GDT_NUM_ENTRIES
};
#else
/* NOTE: If the above enum is modified, this define must be checked */
#define X86_GDT_ENTRY_32BIT_DS	3
#define X86_GDT_NUM_ENTRIES	7
#endif

#define X86_GDT_SIZE		(X86_GDT_NUM_ENTRIES * X86_GDT_ENTRY_SIZE)

#ifndef __ASSEMBLY__

static inline __attribute__((always_inline)) void cpu_hlt(void)
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

/*
 * Generic Extended CPUID function
 */
static inline struct cpuid_result cpuid_ext(int op, unsigned ecx)
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
		: "0" (op), "2" (ecx)
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

/*
 * CPUID functions returning a single datum
 */
static inline unsigned int cpuid_eax(unsigned int op)
{
	unsigned int eax;

	__asm__("mov %%ebx, %%edi;"
		"cpuid;"
		"mov %%edi, %%ebx;"
		: "=a" (eax)
		: "0" (op)
		: "ecx", "edx", "edi");
	return eax;
}

static inline unsigned int cpuid_ebx(unsigned int op)
{
	unsigned int eax, ebx;

	__asm__("mov %%ebx, %%edi;"
		"cpuid;"
		"mov %%ebx, %%esi;"
		"mov %%edi, %%ebx;"
		: "=a" (eax), "=S" (ebx)
		: "0" (op)
		: "ecx", "edx", "edi");
	return ebx;
}

static inline unsigned int cpuid_ecx(unsigned int op)
{
	unsigned int eax, ecx;

	__asm__("mov %%ebx, %%edi;"
		"cpuid;"
		"mov %%edi, %%ebx;"
		: "=a" (eax), "=c" (ecx)
		: "0" (op)
		: "edx", "edi");
	return ecx;
}

static inline unsigned int cpuid_edx(unsigned int op)
{
	unsigned int eax, edx;

	__asm__("mov %%ebx, %%edi;"
		"cpuid;"
		"mov %%edi, %%ebx;"
		: "=a" (eax), "=d" (edx)
		: "0" (op)
		: "ecx", "edi");
	return edx;
}

#define CPU_MAX_NAME_LEN	49

/**
 * cpu_get_name() - Get the name of the current cpu
 *
 * @name: Place to put name, which must be CPU_MAX_NAME_LEN bytes including
 * @return pointer to name, which will likely be a few bytes after the start
 * of @name
 * \0 terminator
 */
char *cpu_get_name(char *name);
#endif /* __ASSEMBLY__ */

#endif
