// SPDX-License-Identifier: GPL-2.0+
/*
 *  EFI application tables support
 *
 *  Copyright (c) 2016 Alexander Graf
 */

#define LOG_CATEGORY LOGC_EFI

#include <common.h>
#include <efi_loader.h>
#include <log.h>
#include <malloc.h>
#include <mapmem.h>
#include <smbios.h>
#include <linux/sizes.h>

enum {
	TABLE_SIZE	= SZ_4K,
};

/*
 * Install the SMBIOS table as a configuration table.
 *
 * Return:	status code
 */
efi_status_t efi_smbios_register(void)
{
	ulong addr;
	efi_status_t ret;

	addr = gd->arch.smbios_start;
	if (!addr) {
		log_err("No SMBIOS tables to install\n");
		return EFI_NOT_FOUND;
	}

	/* Mark space used for tables */
	ret = efi_add_memory_map(addr, TABLE_SIZE, EFI_RUNTIME_SERVICES_DATA);
	if (ret)
		return ret;

	log_debug("EFI using SMBIOS tables at %lx\n", addr);

	/* Install SMBIOS information as configuration table */
	return efi_install_configuration_table(&smbios_guid,
					       map_sysmem(addr, 0));
}

/**
 * find_addr_below() - Find a usable region below the given max_addr
 *
 * Check if *addrp is suitable to define a memory region which finishes below
 * @max_addr + @req_size. If so, do nothing and return 0
 *
 * As a backup, if CONFIG_SMBIOS_TABLE_FIXED is enabled, search for a
 * 4KB-aligned DRAM region which is large enough. Make sure it is below U-Boot's
 * stack space, assumed to be 64KB.
 *
 * @max_addr: Maximum address that can be used (region must finish before here)
 * @req:size: Required size of region
 * @addrp: On entry: Current proposed address; on exit, holds the new address,
 *	on success
 * Return 0 if OK (existing region was OK, or a new one was found), -ENOSPC if
 * nothing suitable was found
 */
static int find_addr_below(ulong max_addr, ulong req_size, ulong *addrp)
{
	struct bd_info *bd = gd->bd;
	ulong max_base;
	int i;

	max_base = max_addr - req_size;
	if (*addrp <= max_base)
		return 0;

	if (!IS_ENABLED(CONFIG_SMBIOS_TABLE_FIXED))
		return -ENOSPC;

	/* Make sure that the base is at least 64KB below the stack */
	max_base = min(max_base,
		       ALIGN(gd->start_addr_sp - SZ_64K - req_size, SZ_4K));

	for (i = CONFIG_NR_DRAM_BANKS - 1; i >= 0; i--) {
		ulong start = bd->bi_dram[i].start;
		ulong size = bd->bi_dram[i].size;
		ulong addr;

		/* chose an address at most req_size bytes before the end */
		addr = min(max_base, start - req_size + size);

		/* check this is in the range */
		if (addr >= start && addr + req_size < start + size) {
			*addrp = addr;
			log_warning("Forcing SMBIOS table to address %lx\n",
				    addr);
			return 0;
		}
	}

	return -ENOSPC;
}

static int install_smbios_table(void)
{
	ulong addr;
	void *buf;

	if (!IS_ENABLED(CONFIG_GENERATE_SMBIOS_TABLE) || IS_ENABLED(CONFIG_X86))
		return 0;

	/* Align the table to a 4KB boundary to keep EFI happy */
	buf = memalign(SZ_4K, TABLE_SIZE);
	if (!buf)
		return log_msg_ret("mem", -ENOMEM);

	addr = map_to_sysmem(buf);

	/*
	 * Deal with a fixed address if needed. For simplicity we assume that
	 * the SMBIOS-table size is <64KB. If a suitable address cannot be
	 * found, then write_smbios_table() returns an error.
	 */
	if (find_addr_below(SZ_4G - 1, SZ_64K, &addr) ||
	    !write_smbios_table(addr)) {
		log_err("Failed to write SMBIOS table\n");
		return log_msg_ret("smbios", -EINVAL);
	}

	/* Make a note of where we put it */
	log_debug("SMBIOS tables written to %lx\n", addr);
	gd->arch.smbios_start = addr;

	return 0;
}
EVENT_SPY_SIMPLE(EVT_LAST_STAGE_INIT, install_smbios_table);
