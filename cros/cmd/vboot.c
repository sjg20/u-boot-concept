// SPDX-License-Identifier: GPL-2.0+
/*
 * Implements the 'vboot' command which provides access to the verified boot
 * flow.
 *
 * Copyright 2018 Google LLC
 */

#include <common.h>
#include <command.h>
#include <dm.h>
#include <cros/nvdata.h>
#include <cros/stages.h>
#include <cros/vboot.h>

/* The next stage of vboot to run (used for repeatable commands) */
static enum vboot_stage_t vboot_next_stage;

int board_run_command(const char *cmd)
{
	struct vboot_info *vboot = vboot_get_alloc();

	printf("Secure boot mode: %s\n", cmd);
	if (!strcmp(cmd, "vboot") || !strcmp(cmd, "vboot_go_auto")) {
		vboot_run_auto(vboot, 0);
		/* Should not return */
	} else {
		printf("Unknown command '%s'\n", cmd);
		panic("board_run_command() failed");
	}

	return 1;
}

static int do_vboot_go(struct cmd_tbl *cmdtp, int flag, int argc,
		       char * const argv[])
{
	struct vboot_info *vboot = vboot_get_alloc();
	const char *stage;
	uint flags = 0;
	int ret;

	/* strip off 'go' */
	argc--;
	argv++;
	if (argc < 1)
		return CMD_RET_USAGE;
	if (!strcmp("-n", argv[0])) {
		flags |= VBOOT_FLAG_CMDLINE;
		argc--;
		argv++;
		if (argc < 1)
			return CMD_RET_USAGE;
	}

	stage = argv[0];
	if (!strcmp(stage, "ro")) {
		ret = vboot_run_stages(vboot, true, flags);
	} else if (!strcmp(stage, "rw")) {
		ret = vboot_run_stages(vboot, false, flags);
	} else if (!strcmp(stage, "auto")) {
		ret = vboot_run_auto(vboot, flags);
	} else {
		enum vboot_stage_t stagenum;

		if (flag & CMD_FLAG_REPEAT) {
			stagenum = vboot_next_stage;
		} else {
			if (!strcmp("start", stage)) {
				stagenum = VBOOT_STAGE_FIRST_VER;
			} else if (!strcmp("start_rw", stage)) {
				stagenum = VBOOT_STAGE_FIRST_RW;
			} else if (!strcmp("next", stage)) {
				stagenum = vboot_next_stage;
			} else {
				stagenum = vboot_find_stage(stage);
				if (stagenum == VBOOT_STAGE_NONE) {
					printf("Umknown stage\n");
					return CMD_RET_USAGE;
				}
			}
		}
		if (stagenum == VBOOT_STAGE_COUNT) {
			printf("All vboot stages are complete\n");
			return 1;
		}

		ret = vboot_run_stage(vboot, stagenum);
		if (!ret)
			vboot_next_stage = stagenum + 1;
	}

	return ret ? 1 : 0;
}

static int do_vboot_list(struct cmd_tbl *cmdtp, int flag, int argc,
			 char * const argv[])
{
	enum vboot_stage_t stagenum;
	const char *name;

	printf("Available stages:\n");
	for (stagenum = VBOOT_STAGE_FIRST_VER; stagenum < VBOOT_STAGE_COUNT;
	     stagenum++) {
		name = vboot_get_stage_name(stagenum);
		printf("   %d: %s\n", stagenum, name);
	}

	return 0;
}

static int dump_nvdata(struct vboot_info *vboot)
{
	struct vb2_context *ctx;
	int ret;

	ctx = vboot_get_ctx(vboot);
	ret = cros_nvdata_read_walk(CROS_NV_DATA, ctx->nvdata,
				    sizeof(ctx->nvdata));
	if (ret)
		return log_msg_ret("read", ret);
	ret = vboot_dump_nvdata(ctx->nvdata, sizeof(ctx->nvdata));
	if (ret)
		return log_msg_ret("dump", ret);

	return 0;
}

static int do_vboot_nvdata(struct cmd_tbl *cmdtp, int flag, int argc,
			   char * const argv[])
{
	struct vboot_info *vboot = vboot_get_alloc();
	int ret;

	ret = dump_nvdata(vboot);
	if (ret) {
		printf("Error %d\n", ret);
		return CMD_RET_FAILURE;
	}

	return 0;
}

static int dump_secdata(struct vboot_info *vboot)
{
	struct vb2_context *ctx;
	int ret;

	ctx = vboot_get_ctx(vboot);
	ret = cros_nvdata_read_walk(CROS_NV_SECDATA, ctx->secdata,
				    sizeof(ctx->secdata));
	if (ret)
		return log_msg_ret("read", ret);
	ret = vboot_dump_secdata(ctx->secdata, sizeof(ctx->secdata));
	if (ret)
		return log_msg_ret("dump", ret);

	return 0;
}

static int do_vboot_secdata(struct cmd_tbl *cmdtp, int flag, int argc,
			   char * const argv[])
{
	struct vboot_info *vboot = vboot_get_alloc();
	int ret;

	ret = dump_secdata(vboot);
	if (ret) {
		printf("Error %d\n", ret);
		return CMD_RET_FAILURE;
	}

	return 0;
}

#ifdef CONFIG_SYS_LONGHELP
static char vboot_help_text[] =
	 "go -n [ro|rw|auto|start|next|<stage>]  Run verified boot stage (repeatable)\n"
	 "vboot list           List verified boot stages\n"
	 "vboot nvdata         Vboot non-volatile data access\n"
	 "vboot secdata        Vboot secure data access";
#endif

U_BOOT_CMD_WITH_SUBCMDS(vboot, "Chromium OS Verified boot", vboot_help_text,
	U_BOOT_CMD_MKENT(go, 4, 0, do_vboot_go, "", ""),
	U_BOOT_CMD_MKENT(list, 4, 0, do_vboot_list, "", ""),
	U_BOOT_CMD_MKENT(nvdata, 4, 0, do_vboot_nvdata, "", ""),
	U_BOOT_CMD_MKENT(secdata, 4, 0, do_vboot_secdata, "", ""),
);

static int do_vboot_go_auto(struct cmd_tbl *cmdtp, int flag, int argc,
			    char * const argv[])
{
	board_run_command("vboot");

	return 0;
}

U_BOOT_CMD(vboot_go_auto, 4, 1, do_vboot_go_auto, "Chromium OS Verified boot",
	   "      Run full verified boot");
