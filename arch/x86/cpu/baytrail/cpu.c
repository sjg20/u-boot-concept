/*
 * Copyright (C) 2015 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <atomic.h>
#include <asm/lapic.h>
#include <asm/mp.h>
#include <asm/msr.h>

enum {
	IACORE_MIN,
	IACORE_LFM,
	IACORE_MAX,
	IACORE_TURBO,
	IACORE_END
};

/* The pattrs structure is a common place to stash pertinent information
 * about the processor or platform. Instead of going to the source (msrs, cpuid)
 * every time an attribute is needed use the pattrs structure.
 */
struct pattrs {
	msr_t platform_id;
	msr_t platform_info;
	int iacore_ratios[IACORE_END];
	int iacore_vids[IACORE_END];
	uint32_t cpuid;
	int revid;
	int stepping;
	const void *microcode_patch;
	int address_bits;
	int num_cpus;
	unsigned bclk_khz;
};

struct pattrs static_pattrs;

static struct mp_flight_record mp_steps[] = {
	MP_FR_BLOCK_APS(mp_initialize_cpu, NULL, mp_initialize_cpu, NULL),
};

/* The APIC id space on Bay Trail is sparse. Each id is separated by 2. */
static int adjust_apic_id(int index, int apic_id)
{
	return 2 * index;
}

static int baytrail_init_cpus(const struct pattrs *pattrs)
{
	struct bus *cpu_bus = NULL;
	struct mp_params mp_params;

	return 0;
// 	x86_mtrr_check();

	/* Enable the local cpu apics */
	lapic_setup();

	mp_params.num_cpus = pattrs->num_cpus,
	mp_params.parallel_microcode_load = 0,
	mp_params.adjust_apic_id = adjust_apic_id;
	mp_params.flight_plan = &mp_steps[0];
	mp_params.num_records = ARRAY_SIZE(mp_steps);
	mp_params.microcode_pointer = 0;

	if (mp_init(cpu_bus, &mp_params)) {
		printf("MP initialization failure.\n");
		return -EIO;
	}

	return 0;
}

int x86_init_cpus(void)
{
	struct pattrs *pattrs = &static_pattrs;

	baytrail_init_cpus(pattrs);

	return 0;
}
