// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2014 Google, Inc
 */

#include <common.h>
#include <asm/msr.h>
#include <asm/mp.h>
#include <asm/mtrr.h>

static const char *const mtrr_type_name[MTRR_TYPE_COUNT] = {
	"Uncacheable",
	"Combine",
	"2",
	"3",
	"Through",
	"Protect",
	"Back",
};

static void save_mtrrs(void *arg)
{
	struct mtrr_info *info = arg;
	int i;

	for (i = 0; i < MTRR_COUNT; i++) {
		info->mtrr[i].base = native_read_msr(MTRR_PHYS_BASE_MSR(i));
		info->mtrr[i].mask = native_read_msr(MTRR_PHYS_MASK_MSR(i));
	}
}

static int do_mtrr_list(int cpu)
{
	struct mtrr_info info;
	int ret;
	int i;

	printf("Reg Valid Write-type   %-16s %-16s %-16s\n", "Base   ||",
	       "Mask   ||", "Size   ||");
	memset(&info, '\0', sizeof(info));
	ret = mp_run_on_cpus(cpu, save_mtrrs, &info);
	if (ret) {
		printf("Failed to run on CPU (err=%d)\n", ret);
		return CMD_RET_FAILURE;
	}
	for (i = 0; i < MTRR_COUNT; i++) {
		const char *type = "Invalid";
		uint64_t base, mask, size;
		bool valid;

		base = info.mtrr[i].base;
		mask = info.mtrr[i].mask;
		size = ~mask & ((1ULL << CONFIG_CPU_ADDR_BITS) - 1);
		size |= (1 << 12) - 1;
		size += 1;
		valid = mask & MTRR_PHYS_MASK_VALID;
		type = mtrr_type_name[base & MTRR_BASE_TYPE_MASK];
		printf("%d   %-5s %-12s %016llx %016llx %016llx\n", i,
		       valid ? "Y" : "N", type, base & ~MTRR_BASE_TYPE_MASK,
		       mask & ~MTRR_PHYS_MASK_VALID, size);
	}

	return 0;
}

static int do_mtrr_set(uint reg, int argc, char * const argv[])
{
	const char *typename = argv[0];
	struct mtrr_state state;
	uint32_t start, size;
	uint64_t base, mask;
	int i, type = -1;
	bool valid;

	if (argc < 3)
		return CMD_RET_USAGE;
	for (i = 0; i < MTRR_TYPE_COUNT; i++) {
		if (*typename == *mtrr_type_name[i])
			type = i;
	}
	if (type == -1) {
		printf("Invalid type name %s\n", typename);
		return CMD_RET_USAGE;
	}
	start = simple_strtoul(argv[1], NULL, 16);
	size = simple_strtoul(argv[2], NULL, 16);

	base = start | type;
	valid = native_read_msr(MTRR_PHYS_MASK_MSR(reg)) & MTRR_PHYS_MASK_VALID;
	mask = ~((uint64_t)size - 1);
	mask &= (1ULL << CONFIG_CPU_ADDR_BITS) - 1;
	if (valid)
		mask |= MTRR_PHYS_MASK_VALID;

	mtrr_open(&state, true);
	wrmsrl(MTRR_PHYS_BASE_MSR(reg), base);
	wrmsrl(MTRR_PHYS_MASK_MSR(reg), mask);
	mtrr_close(&state, true);

	return 0;
}

static int mtrr_set_valid(int reg, bool valid)
{
	struct mtrr_state state;
	uint64_t mask;

	mtrr_open(&state, true);
	mask = native_read_msr(MTRR_PHYS_MASK_MSR(reg));
	if (valid)
		mask |= MTRR_PHYS_MASK_VALID;
	else
		mask &= ~MTRR_PHYS_MASK_VALID;
	wrmsrl(MTRR_PHYS_MASK_MSR(reg), mask);
	mtrr_close(&state, true);

	return 0;
}

static int do_mtrr(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	bool first;
	int cmd;
	int cpu;
	uint reg;
	int i;
	int ret;

	cpu = MP_SELECT_BSP;
	if (argc >= 3 && !strcmp("-c", argv[1])) {
		const char *cpustr;

		cpustr = argv[2];
		if (*cpustr == 'a')
			cpu = MP_SELECT_ALL;
		else
			cpu = simple_strtol(cpustr, NULL, 16);
		argc -= 2;
		argv += 2;
	}
	argc--;
	argv++;
	cmd = argv[0] ? *argv[0] : 0;
	if (argc < 1 || !cmd) {
		cmd = 'l';
		reg = 0;
	} else {
		if (argc < 2)
			return CMD_RET_USAGE;
		reg = simple_strtoul(argv[1], NULL, 16);
		if (reg >= MTRR_COUNT) {
			printf("Invalid register number\n");
			return CMD_RET_USAGE;
		}
	}
	i = mp_first_cpu(cpu);
	if (i < 0) {
		printf("Invalid CPU (err=%d)\n", i);
		return CMD_RET_FAILURE;
	}
	first = true;
	for (; i >= 0; i = mp_next_cpu(cpu, i)) {
		if (!first)
			printf("\n");
		printf("CPU %d:\n", i);
		switch (cmd) {
		case 'l':
			ret = do_mtrr_list(i);
			break;
		case 'e':
			ret = mtrr_set_valid(reg, true);
			break;
		case 'd':
			ret = mtrr_set_valid(reg, false);
			break;
		case 's':
			ret = do_mtrr_set(reg, argc - 2, argv + 2);
			break;
		default:
			return CMD_RET_USAGE;
		}
		first = false;
	}

	return 0;
}

U_BOOT_CMD(
	mtrr,	6,	1,	do_mtrr,
	"Use x86 memory type range registers (32-bit only)",
	"[list]        - list current registers\n"
	"set <reg> <type> <start> <size>   - set a register\n"
	"\t<type> is Uncacheable, Combine, Through, Protect, Back\n"
	"disable <reg>      - disable a register\n"
	"enable <reg>       - enable a register\n"
	"\n"
	"Preceed command with '-c <n>' to access a particular CPU, e.g.\n"
	"   mtrr -c all list; mtrr -c 2e list"
);
