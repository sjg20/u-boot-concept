/*
 * From coreboot file of same name
 *
 * Copyright (C) 2008-2009 coresystems GmbH
 * Copyright (C) 2014 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0
 */
#define DEBUG
#include <common.h>
#include <asm/msr.h>
#include <asm/io.h>
#include <asm/lapic.h>
#include <asm/post.h>

void setup_lapic(void)
{
	/* this is so interrupts work. This is very limited scope --
	 * linux will do better later, we hope ...
	 */
	/* this is the first way we learned to do it. It fails on real SMP
	 * stuff. So we have to do things differently ...
	 * see the Intel mp1.4 spec, page A-3
	 */

#if NEED_LAPIC == 1
	/* Only Pentium Pro and later have those MSR stuff */
	msr_t msr;

	debug("Setting up local apic...");

	/* Enable the local apic */
	msr = msr_read(LAPIC_BASE_MSR);
	msr.lo |= LAPIC_BASE_MSR_ENABLE;
	msr.lo &= ~LAPIC_BASE_MSR_ADDR_MASK;
	msr.lo |= LAPIC_DEFAULT_BASE;
	msr_write(LAPIC_BASE_MSR, msr);

	/*
	 * Set Task Priority to 'accept all'.
	 */
	lapic_write_around(LAPIC_TASKPRI,
		lapic_read_around(LAPIC_TASKPRI) & ~LAPIC_TPRI_MASK);

	/* Put the local apic in virtual wire mode */
	lapic_write_around(LAPIC_SPIV,
		(lapic_read_around(LAPIC_SPIV) & ~(LAPIC_VECTOR_MASK))
		| LAPIC_SPIV_ENABLE);
	lapic_write_around(LAPIC_LVT0,
		(lapic_read_around(LAPIC_LVT0) &
			~(LAPIC_LVT_MASKED | LAPIC_LVT_LEVEL_TRIGGER |
				LAPIC_LVT_REMOTE_IRR | LAPIC_INPUT_POLARITY |
				LAPIC_SEND_PENDING |LAPIC_LVT_RESERVED_1 |
				LAPIC_DELIVERY_MODE_MASK))
		| (LAPIC_LVT_REMOTE_IRR |LAPIC_SEND_PENDING |
			LAPIC_DELIVERY_MODE_EXTINT)
		);
	lapic_write_around(LAPIC_LVT1,
		(lapic_read_around(LAPIC_LVT1) &
			~(LAPIC_LVT_MASKED | LAPIC_LVT_LEVEL_TRIGGER |
				LAPIC_LVT_REMOTE_IRR | LAPIC_INPUT_POLARITY |
				LAPIC_SEND_PENDING |LAPIC_LVT_RESERVED_1 |
				LAPIC_DELIVERY_MODE_MASK))
		| (LAPIC_LVT_REMOTE_IRR |LAPIC_SEND_PENDING |
			LAPIC_DELIVERY_MODE_NMI)
		);

	debug(" apic_id: 0x%02lx ", lapicid());

#else /* !NEED_LLAPIC */
	/* Only Pentium Pro and later have those MSR stuff */
	msr_t msr;

	debug("Disabling local apic...");

	msr = msr_read(LAPIC_BASE_MSR);
	msr.lo &= ~LAPIC_BASE_MSR_ENABLE;
	msr_write(LAPIC_BASE_MSR, msr);
#endif /* !NEED_LAPIC */
	debug("done.\n");
	post_code(0x9b);
}
