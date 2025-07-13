// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2011 The Chromium OS Authors.
 */

#define LOG_CATEGORY	LOGC_SANDBOX

#include <bootm.h>
#include <bootstage.h>
#include <cpu_func.h>
#include <errno.h>
#include <log.h>
#include <mapmem.h>
#include <os.h>
#include <asm/global_data.h>
#include <asm/malloc.h>
#include <asm/state.h>
#include <linux/delay.h>
#include <linux/libfdt.h>

DECLARE_GLOBAL_DATA_PTR;

void __noreturn sandbox_exit(void)
{
	/* Do this here while it still has an effect */
	os_fd_restore();

	if (state_uninit())
		os_exit(2);

	/* This is considered normal termination for now */
	os_exit(0);
}

/* delay x useconds */
void __udelay(unsigned long usec)
{
	struct sandbox_state *state = state_get_current();

	if (!state->skip_delays)
		os_usleep(usec);
}

int cleanup_before_linux(void)
{
	return 0;
}

int cleanup_before_linux_select(int flags)
{
	return 0;
}

void dcache_enable(void)
{
}

void dcache_disable(void)
{
}

int dcache_status(void)
{
	return 1;
}

void flush_dcache_range(unsigned long start, unsigned long stop)
{
}

void invalidate_dcache_range(unsigned long start, unsigned long stop)
{
}

/**
 * setup_auto_tree() - Set up a basic device tree to allow sandbox to work
 *
 * This is used when no device tree is provided. It creates a simple tree with
 * just a /binman node.
 *
 * @blob: Place to put the created device tree
 * Returns: 0 on success, -ve FDT error code on failure
 */
static int setup_auto_tree(void *blob)
{
	int err;

	err = fdt_create_empty_tree(blob, 256);
	if (err)
		return err;

	/* Create a /binman node in case CONFIG_BINMAN is enabled */
	err = fdt_add_subnode(blob, 0, "binman");
	if (err < 0)
		return err;

	return 0;
}

int board_fdt_blob_setup(void **fdtp)
{
	struct sandbox_state *state = state_get_current();
	const char *fname = state->fdt_fname;
	void *blob = NULL;
	loff_t size;
	int err;
	int fd;

	if (gd->fdt_blob)
		return -EEXIST;
	blob = map_sysmem(CONFIG_SYS_FDT_LOAD_ADDR, 0);
	if (!state->fdt_fname) {
		err = setup_auto_tree(blob);
		if (err) {
			os_printf("Unable to create empty FDT: %s\n",
				  fdt_strerror(err));
			return -EINVAL;
		}
		*fdtp = blob;

		return 0;
	}

	err = os_get_filesize(fname, &size);
	if (err < 0) {
		os_printf("Failed to find FDT file '%s'\n", fname);
		return err;
	}
	fd = os_open(fname, OS_O_RDONLY);
	if (fd < 0) {
		os_printf("Failed to open FDT file '%s'\n", fname);
		return -EACCES;
	}

	if (os_read(fd, blob, size) != size) {
		os_close(fd);
		os_printf("Failed to read FDT file '%s'\n", fname);
		return -EIO;
	}
	os_close(fd);

	*fdtp = blob;

	return 0;
}

ulong timer_get_boot_us(void)
{
	static uint64_t base_count;
	uint64_t count = os_get_nsec();

	if (!base_count)
		base_count = count;

	return (count - base_count) / 1000;
}

int sandbox_load_other_fdt(void **fdtp, int *sizep)
{
	const char *orig;
	int ret, size;
	void *fdt = *fdtp;

	ret = state_load_other_fdt(&orig, &size);
	if (ret) {
		log_err("Cannot read other FDT\n");
		return log_msg_ret("ld", ret);
	}

	if (!*fdtp) {
		fdt = os_malloc(size);
		if (!fdt)
			return log_msg_ret("mem", -ENOMEM);
		*sizep = size;
	}

	memcpy(fdt, orig, *sizep);
	*fdtp = fdt;

	return 0;
}
