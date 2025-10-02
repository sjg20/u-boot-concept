// SPDX-License-Identifier: GPL-2.0+
/*
 * video commands
 *
 * Copyright 2022 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <dm.h>
#include <video.h>
#include <video_console.h>

static int do_video_setcursor(struct cmd_tbl *cmdtp, int flag, int argc,
			      char *const argv[])
{
	unsigned int col, row;
	struct udevice *dev;

	if (argc != 3)
		return CMD_RET_USAGE;

	if (uclass_first_device_err(UCLASS_VIDEO_CONSOLE, &dev))
		return CMD_RET_FAILURE;
	col = hextoul(argv[1], NULL);
	row = hextoul(argv[2], NULL);
	vidconsole_position_cursor(dev, col, row);

	return 0;
}

static int do_video_puts(struct cmd_tbl *cmdtp, int flag, int argc,
			 char *const argv[])
{
	struct udevice *dev;
	int ret;

	if (argc != 2)
		return CMD_RET_USAGE;

	if (uclass_first_device_err(UCLASS_VIDEO_CONSOLE, &dev))
		return CMD_RET_FAILURE;
	ret = vidconsole_put_string(dev, argv[1]);
	if (!ret)
		ret = video_sync(dev->parent, false);

	return ret ? CMD_RET_FAILURE : 0;
}

static int do_video_write(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	struct vidconsole_priv *priv;
	bool use_pixels = false;
	struct udevice *dev;
	int ret, i;

	if (argc < 3)
		return CMD_RET_USAGE;

	if (uclass_first_device_err(UCLASS_VIDEO_CONSOLE, &dev))
		return CMD_RET_FAILURE;
	priv = dev_get_uclass_priv(dev);

	/* Check for -p flag */
	if (!strcmp(argv[1], "-p")) {
		use_pixels = true;
		argc--;
		argv++;
	}

	if (argc < 3 || !(argc % 2))
		return CMD_RET_USAGE;

	for (i = 1; i < argc; i += 2) {
		uint col, row;
		char *pos = argv[i];
		char *colon = strchr(pos, ':');

		if (!colon)
			return CMD_RET_USAGE;

		col = hextoul(pos, NULL);
		row = hextoul(colon + 1, NULL);

		if (use_pixels)
			vidconsole_set_cursor_pos(dev, col, row);
		else
			vidconsole_position_cursor(dev, col, row);

		ret = vidconsole_put_string(dev, argv[i + 1]);
		if (ret)
			return CMD_RET_FAILURE;
	}

	ret = video_sync(dev->parent, false);
	if (ret)
		return CMD_RET_FAILURE;

	return 0;
}

U_BOOT_CMD(
	setcurs, 3,	1,	do_video_setcursor,
	"set cursor position within screen",
	"    <col> <row> in hex characters"
);

U_BOOT_CMD(
	lcdputs, 2,	1,	do_video_puts,
	"print string on video framebuffer",
	"    <string>"
);

U_BOOT_LONGHELP(video,
	"setcursor <col> <row>                - Set cursor position\n"
	"video puts <string>                        - Write string at current position\n"
	"video write [-p] [<col>:<row> <string>]... - Write strings at specified positions\n"
	"         -p: Use pixel coordinates instead of character positions");

U_BOOT_CMD_WITH_SUBCMDS(video, "Video commands", video_help_text,
	U_BOOT_SUBCMD_MKENT(setcursor, 3, 1, do_video_setcursor),
	U_BOOT_SUBCMD_MKENT(puts, 2, 1, do_video_puts),
	U_BOOT_SUBCMD_MKENT(write, CONFIG_SYS_MAXARGS, 1, do_video_write));
