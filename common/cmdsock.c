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
#include <pb_decode.h>
#include <pb_encode.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <vsprintf.h>
#include <asm/global_data.h>
#include <cmdsock.pb.h>

DECLARE_GLOBAL_DATA_PTR;

#define BUF_SIZE	4096

static struct cmdsock info, *csi = &info;

#if 0
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
#endif

#if 0
static bool encode_string(pb_ostream_t *stream, const pb_field_iter_t *field, void *const *arg) {
  const char *str = (const char*)*arg;
  return pb_encode_string(stream, (const pb_byte_t*)str, strlen(str));
}

char buf[4096], *ptr;

static bool decode_string(pb_istream_t *stream, const pb_field_iter_t *field,
			  void **arg) {
  // Allocate memory for the string
  // char *str = (char *) malloc(stream->bytes_left + 1); // +1 for null terminator
  // if (str == NULL) {
    // return false; // Or handle memory allocation error
  // }

  // Read the string data from the stream
  char *str = ptr;

  int len = stream->bytes_left;
  if (!pb_read(stream, (void *)ptr, len)) {
    //free(str); // Free the memory
    return false; // Handle read error
  }

  // Null-terminate the string
  str[len] = '\0';
  ptr += len;

  // Store the pointer to the string in the argument
  *arg = str;

  return true;
}
#endif

static int reply(Message *msg)
{
	char *cmd;
	int len;

	len = membuf_putraw(csi->out, BUF_SIZE, false, &cmd);

	pb_ostream_t stream = pb_ostream_from_buffer(cmd, len);
        if (!pb_encode_ex(&stream, Message_fields, msg, PB_ENCODE_DELIMITED)) {
		os_printf("Failed to encode message\n");
#ifndef PB_NO_ERRMSG
		os_printf("msg %s\n", stream.errmsg);
#endif
		os_exit(1);
		return -EIO;
	}

        len = stream.bytes_written;
	// os_printf("wrote %d bytes\n", len);
	membuf_putraw(csi->out, len, true, &cmd);

	// done = true;
	// cmdsock_process();
	cmdsock_poll(csi->in, csi->out);

	return 0;
}

int cmdsock_process(void)
{
        Message req = Message_init_zero;
        Message resp = Message_init_zero;
	int len, ret, used;
	bool send_resp = true;
	char *cmd;

	if (csi->have_err) {
		// ret = reply(csi->out, "fatal_err %d\n", ret);
		// if (!ret)
			// csi->have_err = false;
		return 0;
	}

	/* see if there are commands to process */
	len = membuf_getraw(csi->in, BUF_SIZE, false, &cmd);
	if (!len)
		return 0;

	pb_istream_t stream = pb_istream_from_buffer(cmd, len);

	if (!pb_decode(&stream, Message_fields, &req)) {
		os_printf("Decoding failed: %s\n", PB_GET_ERROR(&stream));
		return 1;
	}

	used = len - stream.bytes_left;
	len = membuf_getraw(csi->in, used, true, &cmd);
	if (!len)
		return 0;

	if (_DEBUG)
		os_printf("cmd: %d\n", req.kind);
	switch (req.which_kind) {
	case Message_start_req_tag:
		os_printf("start: %s\n", req.kind.start_req.name);
		if (csi->inited) {
			resp.kind.start_resp.errcode = -EALREADY;
		} else {
			board_init_f(gd->flags);
			board_init_r(gd->new_gd, 0);
			resp.kind.start_resp.errcode = 0;
			csi->inited = true;
		}
		resp.which_kind = Message_start_resp_tag;
		resp.kind.start_resp.version = 1;
		os_printf("start done\n");
		break;
	case Message_run_cmd_req_tag:
		ret = run_command(req.kind.run_cmd_req.cmd,
				  req.kind.run_cmd_req.flag);
		resp.which_kind = Message_run_cmd_resp_tag;
		resp.kind.run_cmd_resp.result = ret;
		break;
	}

	if (send_resp) {
		ret = reply(&resp);
		if (ret)
			return ret;
	}

#if 0
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
#endif
	// if (ret)
		// csi->have_err = ret;

	return 0;
}

int cmdsock_putc(int ch)
{
	// reply(csi->out, "putc %x\n", ch);

	return 0;
}

int cmdsock_puts(const char *s)
{
	// static bool done;

        Message msg = Message_init_zero;

#if 0
	Message msg = {
		// .which_kind = Message_puts_tag,
		.puts.str = {
			.funcs.encode = encode_string,
			.arg = (char *)s,
		}
	};
#endif
	char *cmd;
	int len;

	// if (done)
		// return 0;
	len = membuf_putraw(csi->out, BUF_SIZE, false, &cmd);
	// os_printf("len %d\n");

	msg.which_kind = Message_puts_tag;
	strlcpy(msg.kind.puts.str, s, sizeof(msg.kind.puts.str));
	// msg.puts.str.funcs.encode = encode_string;
	// msg.puts.str.arg = (char *)s;

	pb_ostream_t stream = pb_ostream_from_buffer(cmd, len);
        if (!pb_encode_ex(&stream, Message_fields, &msg, PB_ENCODE_DELIMITED)) {
		os_printf("Failed to encode message\n");
#ifndef PB_NO_ERRMSG
		os_printf("msg %s\n", stream.errmsg);
#endif
		os_exit(1);
		return -EIO;
	}

        len = stream.bytes_written;
	// os_printf("wrote %d bytes\n", len);
	membuf_putraw(csi->out, len, true, &cmd);

	os_printf("puts: '%s'\n", s);

	// done = true;
	// cmdsock_process();
	cmdsock_poll(csi->in, csi->out);

	// reply(csi->out, "puts %zx %s\n", strlen(s), s);

	return 0;
}

void cmdsock_init(struct membuf *in, struct membuf *out)
{
	csi->in = in;
	csi->out = out;
}
