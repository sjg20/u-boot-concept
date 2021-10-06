// SPDX-License-Identifier: GPL-2.0+
/*
 * distro boot implementation for bootflow
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <blk.h>
#include <bootdevice.h>
#include <command.h>
#include <distro.h>
#include <dm.h>
#include <fs.h>
#include <malloc.h>
#include <mapmem.h>
#include <net.h>
#include <pxe_utils.h>
#include <vsprintf.h>

static int distro_net_getfile(struct pxe_context *ctx, const char *file_path,
			      char *file_addr, ulong *sizep)
{
	char *tftp_argv[] = {"tftp", NULL, NULL, NULL};
	int ret;

	printf("get %s %s\n", file_addr, file_path);
	tftp_argv[1] = file_addr;
	tftp_argv[2] = (void *)file_path;

	if (do_tftpb(ctx->cmdtp, 0, 3, tftp_argv))
		return -ENOENT;
	ret = pxe_get_file_size(sizep);
	if (ret)
		return log_msg_ret("tftp", ret);

	return 0;
}

int distro_boot(struct bootflow *bflow)
{
	struct cmd_tbl cmdtp = {};	/* dummy */
	struct pxe_context ctx;
	struct distro_info info;
	bool is_net = !bflow->blk;
	ulong addr;
	int ret;

	addr = map_to_sysmem(bflow->buf);
	info.bflow = bflow;
	ret = pxe_setup_ctx(&ctx, &cmdtp,
			    is_net ? distro_net_getfile : disto_getfile,
			    &info, !is_net, bflow->subdir);
	if (ret)
		return log_msg_ret("ctx", -EINVAL);

	ret = pxe_process(&ctx, addr, false);
	if (ret)
		return log_msg_ret("bread", -EINVAL);

	return 0;
}
