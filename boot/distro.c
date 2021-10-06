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

/**
 * struct distro_info - useful information for distro_getfile()
 *
 * @bflow: bootflow being booted
 */
struct distro_info {
	struct bootflow *bflow;
};

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

int distro_net_setup(struct bootflow *bflow)
{
	const char *addr_str;
	char fname[200];
	char *bootdir;
	ulong addr;
	ulong size;
	char *buf;
	int ret;

	addr_str = env_get("pxefile_addr_r");
	if (!addr_str)
		return log_msg_ret("pxeb", -EPERM);
	addr = simple_strtoul(addr_str, NULL, 16);

	bflow->type = BOOTFLOWT_DISTRO;
	ret = pxe_get(addr, &bootdir, &size);
	if (ret)
		return log_msg_ret("pxeb", ret);
	bflow->size = size;

	/* Use the directory of the dhcp bootdir as our subdir, if provided */
	if (bootdir) {
		const char *last_slash;
		int path_len;

		last_slash = strrchr(bootdir, '/');
		if (last_slash) {
			path_len = (last_slash - bootdir) + 1;
			bflow->subdir = malloc(path_len + 1);
			memcpy(bflow->subdir, bootdir, path_len);
			bflow->subdir[path_len] = '\0';
		}
	}
	snprintf(fname, sizeof(fname), "%s%s",
		 bflow->subdir ? bflow->subdir : "", DISTRO_FNAME);

	bflow->fname = strdup(fname);
	if (!bflow->fname)
		return log_msg_ret("name", -ENOMEM);

	bflow->state = BOOTFLOWST_LOADED;

	/* Allocate the buffer, including the \0 byte added by get_pxe_file() */
	buf = malloc(size + 1);
	if (!buf)
		return log_msg_ret("buf", -ENOMEM);
	memcpy(buf, map_sysmem(addr, 0), size + 1);
	bflow->buf = buf;

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
