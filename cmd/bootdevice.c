// SPDX-License-Identifier: GPL-2.0+
/*
 * 'bootdevice' command
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootdevice.h>
#include <command.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>

static int bootdevice_check_state(struct bootflow_state **statep)
{
	struct bootflow_state *state;
	int ret;

	ret = bootdevice_get_state(&state);
	if (ret)
		return ret;
	if (!state->cur_bootdevice) {
		printf("Please use 'bootdevice select' first\n");
		return -ENOENT;
	}
	*statep = state;

	return 0;
}

static int do_bootdevice_list(struct cmd_tbl *cmdtp, int flag, int argc,
			      char *const argv[])
{
	bool probe;

	probe = argc >= 2 && !strcmp(argv[1], "-p");
	bootdevice_list(probe);

	return 0;
}

static int do_bootdevice_select(struct cmd_tbl *cmdtp, int flag, int argc,
				char *const argv[])
{
	struct bootflow_state *state;
	struct udevice *dev;
	const char *name;
	char *endp;
	int seq;
	int ret;

	ret = bootdevice_get_state(&state);
	if (ret)
		return CMD_RET_FAILURE;
	if (argc < 2) {
		state->cur_bootdevice = NULL;
		return 0;
	}
	name = argv[1];
	seq = simple_strtol(name, &endp, 16);

	/* Select by name or number */
	if (*endp)
		ret = uclass_get_device_by_name(UCLASS_BOOTDEVICE, name, &dev);
	else
		ret = uclass_get_device_by_seq(UCLASS_BOOTDEVICE, seq, &dev);
	if (ret) {
		printf("Cannot find '%s' (err=%d)\n", name, ret);
		return CMD_RET_FAILURE;
	}
	state->cur_bootdevice = dev;

	return 0;
}

static int do_bootdevice_info(struct cmd_tbl *cmdtp, int flag, int argc,
			      char *const argv[])
{
	struct bootflow_state *state;
	struct bootflow *bflow;
	int ret, i, num_valid;
	struct udevice *dev;
	bool probe;

	probe = argc >= 2 && !strcmp(argv[1], "-p");

	ret = bootdevice_check_state(&state);
	if (ret)
		return CMD_RET_FAILURE;

	dev = state->cur_bootdevice;

	/* Count the number of bootflows, including how many are valid*/
	num_valid = 0;
	for (ret = bootdevice_first_bootflow(dev, &bflow), i = 0;
	     !ret;
	     ret = bootdevice_next_bootflow(&bflow), i++)
		num_valid += bflow->state == BOOTFLOWST_LOADED;

	/*
	 * Prove the device, if requested, otherwise assume that there is no
	 * error
	 */
	ret = 0;
	if (probe)
		ret = device_probe(dev);

	printf("Name:      %s\n", dev->name);
	printf("Sequence:  %d\n", dev_seq(dev));
	printf("Status:    %s\n", ret ? simple_itoa(ret) : device_active(dev) ?
		"Probed" : "OK");
	printf("Uclass:    %s\n", dev_get_uclass_name(dev_get_parent(dev)));
	printf("Bootflows: %d (%d valid)\n", i, num_valid);

	return 0;
}

#ifdef CONFIG_SYS_LONGHELP
static char bootdevice_help_text[] =
	"list [-p]      - list all available bootdevices (-p to probe)\n"
	"bootdevice select <bm>    - select a bootdevice by name\n"
	"bootdevice info [-p]      - show information about a bootdevice (-p to probe)";
#endif

U_BOOT_CMD_WITH_SUBCMDS(bootdevice, "Bootdevices", bootdevice_help_text,
	U_BOOT_SUBCMD_MKENT(list, 2, 1, do_bootdevice_list),
	U_BOOT_SUBCMD_MKENT(select, 2, 1, do_bootdevice_select),
	U_BOOT_SUBCMD_MKENT(info, 2, 1, do_bootdevice_info));
