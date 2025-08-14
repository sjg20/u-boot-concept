// SPDX-License-Identifier: GPL-2.0+
/*
 * Aurora Innovation, Inc. Copyright 2022.
 *
 */

#include <blk.h>
#include <config.h>
#include <command.h>
#include <env.h>
#include <fs_legacy.h>
#include <lmb.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

int do_addr_find(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	const char *filename;
	loff_t size;
	ulong addr;
	int ret;

	if (!gd->fdt_blob) {
		log_err("No FDT setup\n");
		return CMD_RET_FAILURE;
	}

	if (fs_set_blk_dev(argv[1], argc >= 3 ? argv[2] : NULL, FS_TYPE_ANY)) {
		log_err("Can't set block device\n");
		return CMD_RET_FAILURE;
	}

	if (argc >= 4) {
		filename = argv[3];
	} else {
		filename = env_get("bootfile");
		if (!filename) {
			log_err("No boot file defined\n");
			return CMD_RET_FAILURE;
		}
	}

	ret = fs_size(filename, &size);
	if (ret != 0) {
		log_err("Failed to get file size\n");
		return CMD_RET_FAILURE;
	}

	addr = lmb_alloc(size, SZ_1M);
	if (!addr) {
		log_err("Failed to find enough RAM for 0x%llx bytes\n", size);
		return CMD_RET_FAILURE;
	}

	if (env_set_hex("loadaddr", addr)) {
		log_err("Could not set loadaddr\n");
		return CMD_RET_FAILURE;
	}

	log_debug("Set loadaddr to %lx\n", addr);

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	addr_find, 7, 1, do_addr_find,
	"find a load address suitable for a file",
	"<interface> [<dev[:part]>] <filename>\n"
	"- find a consecutive region of memory sufficiently large to hold\n"
	"  the file called 'filename' from 'dev' on 'interface'. If\n"
	"  successful, 'loadaddr' will be set to the located address."
);
