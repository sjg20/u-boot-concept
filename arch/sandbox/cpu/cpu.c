/*
 * Copyright (c) 2011 The Chromium OS Authors.
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm/root.h>
#include <os.h>
#include <asm/io.h>
#include <asm/state.h>

DECLARE_GLOBAL_DATA_PTR;

void reset_cpu(ulong ignored)
{
	if (state_uninit())
		os_exit(2);

	if (dm_uninit())
		os_exit(2);

	/* This is considered normal termination for now */
	os_exit(0);
}

int do_reset(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	reset_cpu(0);

	return 0;
}

/* delay x useconds */
void __udelay(unsigned long usec)
{
	os_usleep(usec);
}

unsigned long __attribute__((no_instrument_function)) timer_get_us(void)
{
	return os_get_nsec() / 1000;
}

int do_bootm_linux(int flag, int argc, char *argv[], bootm_headers_t *images)
{
	if (flag & (BOOTM_STATE_OS_GO | BOOTM_STATE_OS_FAKE_GO)) {
		bootstage_mark(BOOTSTAGE_ID_RUN_OS);
		printf("## Transferring control to Linux (at address %08lx)...\n",
		       images->ep);
		reset_cpu(0);
	}

	return 0;
}

int cleanup_before_linux(void)
{
	return 0;
}

void *map_physmem(phys_addr_t paddr, unsigned long len, unsigned long flags)
{
	return (void *)(gd->arch.ram_buf + paddr);
}

phys_addr_t map_to_sysmem(const void *ptr)
{
	return (u8 *)ptr - gd->arch.ram_buf;
}

void flush_dcache_range(unsigned long start, unsigned long stop)
{
}

int sandbox_read_fdt_from_file(void)
{
	struct sandbox_state *state = state_get_current();
	const char *fname = state->fdt_fname;
	void *blob;
	loff_t size;
	int err;
	int fd;

	blob = map_sysmem(CONFIG_SYS_FDT_LOAD_ADDR, 0);
	if (!state->fdt_fname) {
		err = fdt_create_empty_tree(blob, 256);
		if (!err)
			goto done;
		printf("Unable to create empty FDT: %s\n", fdt_strerror(err));
		return -EINVAL;
	}

	err = os_get_filesize(fname, &size);
	if (err < 0) {
		printf("Failed to file FDT file '%s'\n", fname);
		return err;
	}
	fd = os_open(fname, OS_O_RDONLY);
	if (fd < 0) {
		printf("Failed to open FDT file '%s'\n", fname);
		return -EACCES;
	}
	if (os_read(fd, blob, size) != size) {
		os_close(fd);
		return -EIO;
	}
	os_close(fd);

done:
	gd->fdt_blob = blob;

	return 0;
}
