/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Provides a way to communicate with sandbox from another process
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#define LOG_DEBUG
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
		printf("error: empty reply\n");
		csi->have_err = -EINVAL;
		return -EINVAL;
	}
	if (len >= space - 1) {
		printf("error: no space for reply (space %d len %d)\n",
			  space, len);
		csi->have_err = -ENOSPC;
		return -ENOSPC;
	}
	if (_DEBUG)
		printf("reply: %s\n", ptr);
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

	log_debug("reply kind %d\n", msg->which_kind);
	len = membuf_putraw(csi->out, BUF_SIZE, false, &cmd);

	pb_ostream_t stream = pb_ostream_from_buffer(cmd, len);
        if (!pb_encode_ex(&stream, Message_fields, msg, PB_ENCODE_DELIMITED)) {
		printf("Failed to encode message\n");
#ifndef PB_NO_ERRMSG
		printf("msg %s\n", stream.errmsg);
#endif
		os_exit(1);
		return -EIO;
	}

        len = stream.bytes_written;
	// printf("wrote %d bytes\n", len);
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
	bool old_capture;
	char *cmd;

	old_capture = csi->capture;
	csi->capture = false;

	if (csi->have_err) {
		// ret = reply(csi->out, "fatal_err %d\n", ret);
		// if (!ret)
			// csi->have_err = false;
		goto done;
	}

	/* see if there are commands to process */
	len = membuf_getraw(csi->in, BUF_SIZE, false, &cmd);
	if (!len)
		goto done;

	pb_istream_t stream = pb_istream_from_buffer(cmd, len);

	log_debug("processing\n");
	if (!pb_decode(&stream, Message_fields, &req)) {
		log_err("Decoding failed: %s\n", PB_GET_ERROR(&stream));
		goto fail;
	}

	used = len - stream.bytes_left;
	len = membuf_getraw(csi->in, used, true, &cmd);
	if (!len)
		goto done;

	log_debug("cmd: %d\n", req.which_kind);
	switch (req.which_kind) {
	case Message_start_req_tag:
		printf("start: %s\n", req.kind.start_req.name);
		if (csi->inited) {
			resp.kind.start_resp.errcode = -EALREADY;
		} else {
			csi->capture = true;
			board_init_f(gd->flags);
			board_init_r(gd->new_gd, 0);
			csi->capture = false;
			resp.kind.start_resp.errcode = 0;
			csi->inited = true;
		}
		resp.which_kind = Message_start_resp_tag;
		resp.kind.start_resp.version = 1;
		printf("start done: %s\n", req.kind.start_req.name);
		break;
	case Message_run_cmd_req_tag:
		csi->capture = true;
		ret = run_command(req.kind.run_cmd_req.cmd,
				  req.kind.run_cmd_req.flag);
		csi->capture = false;
		resp.which_kind = Message_run_cmd_resp_tag;
		resp.kind.run_cmd_resp.result = ret;
		break;
	}

	if (send_resp) {
		ret = reply(&resp);
		if (ret)
			goto fail;
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
done:
	csi->capture = old_capture;

	return 0;
fail:
	printf("cmdsock fail\n");
	csi->capture = old_capture;
	return -EINVAL;
}

int cmdsock_putc(int ch)
{
	if (!csi->capture)
		return -1;
	// reply(csi->out, "putc %x\n", ch);

	return 0;
}

int cmdsock_puts(const char *s, int len)
{
	// static bool done;

	if (!csi->capture)
		return -1;
	csi->capture = false;

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
	int spc;

	// if (done)
		// return 0;
	spc = membuf_putraw(csi->out, BUF_SIZE, false, &cmd);
	// printf("spc %d\n");

	if (len >= sizeof(msg.kind.puts.str))
		len = sizeof(msg.kind.puts.str) - 1;
	msg.which_kind = Message_puts_tag;
	strlcpy(msg.kind.puts.str, s, len + 1);
	// msg.puts.str.funcs.encode = encode_string;
	// msg.puts.str.arg = (char *)s;

	pb_ostream_t stream = pb_ostream_from_buffer(cmd, spc);
        if (!pb_encode_ex(&stream, Message_fields, &msg, PB_ENCODE_DELIMITED)) {
		printf("Failed to encode message\n");
#ifndef PB_NO_ERRMSG
		printf("msg %s\n", stream.errmsg);
#endif
		os_exit(1);
		return -EIO;
	}

        spc = stream.bytes_written;
	// printf("wrote %d bytes\n", spc);
	membuf_putraw(csi->out, spc, true, &cmd);

	printf("puts: '%s'\n", s);

	// done = true;
	// cmdsock_process();
	cmdsock_poll(csi->in, csi->out);

	// reply(csi->out, "puts %zx %s\n", strlen(s), s);
	csi->capture = true;

	return len;
}

void cmdsock_init(struct membuf *in, struct membuf *out)
{
	csi->in = in;
	csi->out = out;
	log_debug("cmdsock_init\n");
}
