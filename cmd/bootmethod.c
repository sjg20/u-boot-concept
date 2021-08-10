// SPDX-License-Identifier: GPL-2.0+
/*
 * 'bootmethod' command
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootmethod.h>
#include <command.h>
#include <dm.h>
#include <dm/uclass-internal.h>

// TODO: Can we allow the samdbox test to reset this?
struct g_bootmethod {
	struct udevice *cur_dev;
} g_bootmethod;

static int get_cur_bootmethod(struct udevice **devp)
{
	if (!g_bootmethod.cur_dev) {
		printf("Please use 'bootmethod select' first\n");
		return -ENOENT;
	}
	*devp = g_bootmethod.cur_dev;

	return 0;
}

static int do_bootmethod_list(struct cmd_tbl *cmdtp, int flag, int argc,
			      char *const argv[])
{
	bool probe;

	probe = argc >= 2 && !strcmp(argv[1], "-p");
	bootmethod_list(probe);

	return 0;
}

static int do_bootmethod_select(struct cmd_tbl *cmdtp, int flag, int argc,
				char *const argv[])
{
	struct udevice *dev;
	const char *name;
	char *endp;
	int seq;
	int ret;

	if (argc < 2) {
		g_bootmethod.cur_dev = NULL;
		return 0;
	}
	name = argv[1];
	seq = simple_strtol(name, &endp, 16);
	if (*endp)
		ret = uclass_get_device_by_name(UCLASS_BOOTMETHOD, name, &dev);
	else
		ret = uclass_get_device_by_seq(UCLASS_BOOTMETHOD, seq, &dev);
	if (ret) {
		printf("Cannot find '%s' (err=%d)\n", name, ret);
		return CMD_RET_FAILURE;
	}
	g_bootmethod.cur_dev = dev;

	return 0;
}

static int do_bootmethod_info(struct cmd_tbl *cmdtp, int flag, int argc,
			      char *const argv[])
{
	struct udevice *dev;

	if (get_cur_bootmethod(&dev))
		return CMD_RET_FAILURE;
	printf("%s\n", dev->name);

	return 0;
}

static int do_bootmethod_bootflows(struct cmd_tbl *cmdtp, int flag, int argc,
			      char *const argv[])
{
	struct bootflow bflow;
	struct udevice *dev;
	int num_valid;
	int ret, i;
	bool all;

	all = argc >= 2 && !strcmp(argv[1], "-a");

	if (get_cur_bootmethod(&dev))
		return CMD_RET_FAILURE;
	printf("Seq   State  Part  Name            Filename\n");
	printf("---  ------  ----  --------------  ----------------\n");
	for (i = 0, ret = 0, num_valid = 0; i < 100 && ret != -ESHUTDOWN; i++) {
		ret = bootmethod_get_bootflow(dev, i, &bflow);
		if ((ret && !all) || ret == -ESHUTDOWN)
			continue;
		num_valid++;
		printf("%3x  %6s  %4x  %-14s  %s\n", i,
		       bootmethod_state_get_name(bflow.state), bflow.part,
		       bflow.name, bflow.fname);
	}
	printf("---  ------  ----  --------------  ----------------\n");
	printf("(%d bootflow%s, %d valid)\n", i, i != 1 ? "s" : "", num_valid);


	return 0;
}

#ifdef CONFIG_SYS_LONGHELP
static char bootmethod_help_text[] =
	"list [-p]      - list all available bootmethods (-p to probe)\n"
	"bootmethod select <bm>    - select a bootmethod by name\n"
	"bootmethod info           - show information about a bootmethod\n"
	"bootmethod bootflows [-a] - show bootflows (-a for all)";
#endif

U_BOOT_CMD_WITH_SUBCMDS(bootmethod, "Bootmethods", bootmethod_help_text,
	U_BOOT_SUBCMD_MKENT(list, 2, 1, do_bootmethod_list),
	U_BOOT_SUBCMD_MKENT(select, 2, 1, do_bootmethod_select),
	U_BOOT_SUBCMD_MKENT(info, 1, 1, do_bootmethod_info),
	U_BOOT_SUBCMD_MKENT(bootflows, 1, 1, do_bootmethod_bootflows));
