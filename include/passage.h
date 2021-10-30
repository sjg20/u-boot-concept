/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Standard passage implementation
 *
 * Copyright 2022 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __PASSAGE_H
#define __PASSAGE_H

enum {
	PASSAGE_ABI_MACH	= 0xb00757a3,
	PASSAGE_ABI_VERSION	= 1,
};

static inline ulong passage_mach_version(void)
{
#if BITS_PER_LONG == 64
	return (ulong)PASSAGE_ABI_MACH << 32 | PASSAGE_ABI_VERSION;
#else
	return (PASSAGE_ABI_MACH & ~0xff) | PASSAGE_ABI_VERSION;
#endif
}

void __noreturn arch_passage_entry(ulong entry_addr, ulong bloblist, ulong fdt);

#endif
