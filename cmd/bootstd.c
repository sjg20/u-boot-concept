// SPDX-License-Identifier: GPL-2.0+
/*
 * 'bootstd' command
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <bootdev.h>
#include <bootmeth.h>
#include <bootstd.h>
#include <command.h>
#include <dm.h>
#include <malloc.h>
#include <dm/uclass-internal.h>

static int do_bootstd_images(struct cmd_tbl *cmdtp, int flag, int argc,
			     char *const argv[])
{
	const struct bootstd_img *img;
	struct bootstd_priv *std;
	int ret;

	ret = bootstd_get_priv(&std);
	if (ret) {
		printf("Cannot get bootstd (err=%d)\n", ret);
		return CMD_RET_FAILURE;
	}

	printf("Seq  Bootflow      Type             At      Size  Filename\n");
	printf("---  --------      ---------  --------  --------  --------\n");

	/*
	 * Use the ordering if we have one, so long as we are not trying to list
	 * all bootmethds
	 */
	alist_for_each(img, &std->images) {
	}
	printf("---  --------      ---------  --------  --------  --------\n");
	printf("(%d image%s)\n", std->images.count,
	       std->images.count != 1 ? "s" : "");

	return 0;
}

U_BOOT_LONGHELP(bootstd,
	"images      - list loaded images");

U_BOOT_CMD_WITH_SUBCMDS(bootstd, "Standard-boot operation", bootstd_help_text,
	U_BOOT_SUBCMD_MKENT(images, 1, 1, do_bootstd_images));
