// SPDX-License-Identifier: GPL-2.0+
/*
 * 'blk' command
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <command.h>
#include <dm.h>
#include <part.h>

static int do_blk_list(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	struct udevice *dev;
	struct uclass *uc;

	uclass_id_foreach_dev(UCLASS_BLK, dev, uc) {
		struct blk_desc *desc = dev_get_uclass_plat(dev);

		printf("Device %d: ", dev_seq(dev->parent));
		dev_print(desc);
	}

	return 0;
}

#ifdef CONFIG_SYS_LONGHELP
static char blk_help_text[] =
	"list      - list all block devices";
#endif

U_BOOT_CMD_WITH_SUBCMDS(blk, "Block devices", blk_help_text,
	U_BOOT_SUBCMD_MKENT(list, 2, 1, do_blk_list));
