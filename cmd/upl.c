// SPDX-License-Identifier: GPL-2.0+
/*
 * Commands for UPL handoff generation
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY UCLASS_BOOTSTD

#include <abuf.h>
#include <alist.h>
#include <command.h>
#include <display_options.h>
#include <mapmem.h>
#include <string.h>
#include <upl.h>
#include <dm/ofnode.h>
#include <test/ut.h>

DECLARE_GLOBAL_DATA_PTR;

static int do_upl_info(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	const struct upl *upl = gd_upl();

	printf("UPL state: %sactive\n", upl ? "" : "in");
	if (!upl)
		return 0;
	if (argc > 1 && !strcmp("-v", argv[1])) {
		int i;

		printf("fit %llx size %lx\n", upl->fit.base, upl->fit.size);
		printf("conf_offset %x\n", upl->conf_offset);
		for (i = 0; i < upl->image.count; i++) {
			const struct upl_image *img =
				alist_get(&upl->image, i, struct upl_image);

			printf("image %d: load %llx size %lx entry %lx offset %x: %s\n",
			       i, img->reg.base, img->reg.size, img->entry, img->offset,
			       img->description);
		}
	}

	return 0;
}

static int do_upl_write(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	struct upl s_upl, *upl = &s_upl;
	struct unit_test_state uts = { 0 };
	struct abuf buf;
	oftree tree;
	ulong addr;
	int ret;

	if (!IS_ENABLED(CONFIG_UNIT_TEST)) {
		printf("Not yet implemented\n");
		return CMD_RET_FAILURE;
	}

	upl_get_test_data(&uts, upl);

	log_debug("Writing UPL\n");
	ret = upl_create_handoff_tree(upl, &tree);
	if (ret) {
		log_err("Failed to write (err=%dE)\n", ret);
		return CMD_RET_FAILURE;
	}

	log_debug("Flattening\n");
	ret = oftree_to_fdt(tree, &buf);
	if (ret) {
		log_err("Failed to write (err=%dE)\n", ret);
		return CMD_RET_FAILURE;
	}
	addr = map_to_sysmem(abuf_data(&buf));
	printf("UPL handoff written to %lx size %zx\n", addr, abuf_size(&buf));
	if (env_set_hex("upladdr", addr) ||
	    env_set_hex("uplsize", abuf_size(&buf))) {
		printf("Cannot set env var\n");
		return CMD_RET_FAILURE;
	}

	log_debug("done\n");

	return 0;
}

static int do_upl_read(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	struct upl s_upl, *upl = &s_upl;
	oftree tree;
	ulong addr;
	int ret;

	if (argc < 1)
		return CMD_RET_USAGE;
	addr = hextoul(argv[1], NULL);

	printf("Reading UPL at %lx\n", addr);
	tree = oftree_from_fdt(map_sysmem(addr, 0));
	ret = upl_read_handoff(upl, tree);
	if (ret) {
		log_err("Failed to read (err=%dE)\n", ret);
		return CMD_RET_FAILURE;
	}

	return 0;
}

static int do_upl_exec(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	ulong addr;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	addr = hextoul(argv[1], NULL);
	ret = upl_exec(addr);
	if (ret) {
		printf("Failed (err=%dE)\n", ret);
		return CMD_RET_FAILURE;
	}

	return 0;
}

U_BOOT_LONGHELP(upl,
	"info [-v]     - Check UPL status\n"
	"upl read <addr>   - Read handoff information\n"
	"upl write         - Write handoff information\n"
	"upl exec          - Execute a loaded UPL");

U_BOOT_CMD_WITH_SUBCMDS(upl, "Universal Payload support", upl_help_text,
	U_BOOT_SUBCMD_MKENT(info, 2, 1, do_upl_info),
	U_BOOT_SUBCMD_MKENT(read, 2, 1, do_upl_read),
	U_BOOT_SUBCMD_MKENT(write, 1, 1, do_upl_write),
	U_BOOT_SUBCMD_MKENT(exec, 2, 1, do_upl_exec));
