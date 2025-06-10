/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Provides a way to communicate with sandbox from another process
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY	LOGC_TEST

#include <cmdsock.h>
#include <command.h>
#include <errno.h>
#include <init.h>
#include <log.h>
#include <membuf.h>
#include <os.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <vsprintf.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

static struct cmdsock info, *csi = &info;

static int __attribute__ ((format (__printf__, 2, 3)))
	reply(struct membuf *out, const char *fmt, ...)
{
	int space, len;
	va_list args;
	char *ptr;

	space = membuf_putraw(out, CMDSOCK_BUF_SIZE, false, &ptr);
	va_start(args, fmt);
	len = vsnprintf(ptr, space - 1, fmt, args);
	va_end(args);
	if (!len) {
		os_printf("error: empty reply\n");
		csi->have_err = -EINVAL;
		return -EINVAL;
	}
	if (len >= space - 1) {
		os_printf("error: no space for reply (space %d len %d)\n",
			  space, len);
		csi->have_err = -ENOSPC;
		return -ENOSPC;
	}
	if (_DEBUG)
		os_printf("reply: %s\n", ptr);
	ptr[len - 1] = '\n';
	membuf_putraw(out, len, true, &ptr);

	return 0;
}

int cmdsock_process(void)
{
	char str[4096], *p;
	const char *cmd;
	int len, ret;

	if (csi->have_err) {
		ret = reply(csi->out, "fatal_err %d\n", ret);
		if (!ret)
			csi->have_err = false;
		return 0;
	}

	/* see if there are commands to process */
	len = membuf_readline(csi->in, str, sizeof(str), '\0', false);
	// printf("avail %d len %d\n", membuf_avail(csi->in), len);
	if (!len)
		return 0;

	if (_DEBUG)
		os_printf("cmd: %s\n", str);
	p = str;
	cmd = strsep(&p, " ");
	if (!strcmp("start", cmd)) {
		/* Do pre- and post-relocation init */
		board_init_f(gd->flags);
		board_init_r(gd->new_gd, 0);
		reply(csi->out, "started %d\n", 0);
	} else if (!strcmp("cmd", cmd)) {
		ret = run_command(p, 0);
		reply(csi->out, "result %d\n", ret);
	} else {
		reply(csi->out, "invalid %d\n", -ENOENT);
	}
	if (ret)
		csi->have_err = ret;

	return 0;
}

int cmdsock_putc(int ch)
{
	reply(csi->out, "putc %x\n", ch);

	return 0;
}

int cmdsock_puts(const char *s)
{
	reply(csi->out, "puts %zx %s\n", strlen(s), s);

	return 0;
}

void cmdsock_init(struct membuf *in, struct membuf *out)
{
	csi->in = in;
	csi->out = out;
}
