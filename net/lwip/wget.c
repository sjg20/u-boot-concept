// SPDX-License-Identifier: GPL-2.0+
/* Copyright (C) 2024 Linaro Ltd. */

#include <command.h>
#include <console.h>
#include <display_options.h>
#include <image.h>
#include <lwip/apps/http_client.h>
#include <lwip/timeouts.h>
#include <net.h>
#include <stdio.h>
#include <time.h>
#include "lwip/altcp_tls.h"
#include "mbedtls/debug.h"

#define SERVER_NAME_SIZE 200
#define HTTP_PORT_DEFAULT 80
#define HTTPS_PORT_DEFAULT 443
#define HTTP_SCHEME "http://"
#define HTTPS_SCHEME "https://"

#define PROGRESS_PRINT_STEP_BYTES (100 * 1024)

enum done_state {
	NOT_DONE = 0,
	SUCCESS = 1,
	FAILURE = 2
};

struct wget_ctx {
	ulong daddr;
	ulong saved_daddr;
	ulong size;
	ulong prevsize;
	ulong start_time;
	enum done_state done;
};

static int parse_url(char *url, char *host, u16 *port, char **path)
{
	char *p, *pp;
	long lport;
	bool https = false;

	p = strstr(url, HTTPS_SCHEME);
	if (!p) {
		p = strstr(url, HTTP_SCHEME);
		p += strlen(HTTP_SCHEME);
		if (!p) {
			log_err("only http:// and https:// are supported\n");
			return -ENOENT;
		}
	} else {
		p += strlen(HTTPS_SCHEME);
		https = true;
	}

	/* Parse hostname */
	pp = strchr(p, ':');
	if (!pp)
		pp = strchr(p, '/');
	if (!pp)
		return -EINVAL;

	if (p + SERVER_NAME_SIZE <= pp)
		return -EINVAL;

	memcpy(host, p, pp - p);
	host[pp - p] = '\0';

	if (*pp == ':') {
		/* Parse port number */
		p = pp + 1;
		lport = simple_strtol(p, &pp, 10);
		if (pp && *pp != '/')
			return -EINVAL;
		if (lport > 65535)
			return -EINVAL;
		*port = (u16)lport;
	} else if (https) {
		*port = HTTPS_PORT_DEFAULT;
	} else {
		*port = HTTP_PORT_DEFAULT;
	}
	if (*pp != '/')
		return -EINVAL;
	*path = pp;

	return 0;
}

bool wget_validate_uri(char *uri)
{
	if (strstr(uri, HTTP_SCHEME) || strstr(uri, HTTPS_SCHEME)) {
		return true;
	}

	log_err("only http:// and https:// are supported\n");
	return false;
}

static err_t httpc_recv_cb(void *arg, struct altcp_pcb *pcb, struct pbuf *pbuf,
			   err_t err)
{
	struct wget_ctx *ctx = arg;
	struct pbuf *buf;

	if (!pbuf)
		return ERR_BUF;

	for (buf = pbuf; buf; buf = buf->next) {
		memcpy((void *)ctx->daddr, buf->payload, buf->len);
		ctx->daddr += buf->len;
		ctx->size += buf->len;
		if (ctx->size - ctx->prevsize > PROGRESS_PRINT_STEP_BYTES) {
			printf("#");
			ctx->prevsize = ctx->size;
		}
	}

	altcp_recved(pcb, pbuf->tot_len);
	pbuf_free(pbuf);
	return ERR_OK;
}

static void httpc_result_cb(void *arg, httpc_result_t httpc_result,
				u32_t rx_content_len, u32_t srv_res, err_t err)
{
	struct wget_ctx *ctx = arg;
	ulong elapsed;

	if (httpc_result != HTTPC_RESULT_OK) {
		log_err("\nHTTP client error %d\n", httpc_result);
		ctx->done = FAILURE;
		return;
	}

	elapsed = get_timer(ctx->start_time);
	if (rx_content_len > PROGRESS_PRINT_STEP_BYTES)
		printf("\n");

	printf("%u bytes transferred in %lu ms (", rx_content_len,
		   get_timer(ctx->start_time));
	print_size(rx_content_len / elapsed * 1000, "/s)\n");

	if (env_set_hex("filesize", rx_content_len) ||
		env_set_hex("fileaddr", ctx->saved_daddr)) {
		log_err("Could not set filesize or fileaddr\n");
		ctx->done = FAILURE;
		return;
	}

	ctx->done = SUCCESS;
}

static int wget_loop(struct udevice *udev, ulong dst_addr, char *uri)
{
	char server_name[SERVER_NAME_SIZE];
	httpc_connection_t conn;
	httpc_state_t *state;
	struct netif *netif;
	struct wget_ctx ctx;
	char *path;
	u16 port;

	ctx.daddr = dst_addr;
	ctx.saved_daddr = dst_addr;
	ctx.done = NOT_DONE;
	ctx.size = 0;
	ctx.prevsize = 0;

	if (parse_url(uri, server_name, &port, &path))
		return CMD_RET_USAGE;

	netif = net_lwip_new_netif(udev);
	if (!netif)
		return -1;

	memset(&conn, 0, sizeof(conn));
	conn.result_fn = httpc_result_cb;
	ctx.start_time = get_timer(0);

	if (port == HTTPS_PORT_DEFAULT) {
		altcp_allocator_t tls_allocator;

		tls_allocator.alloc = &altcp_tls_alloc;
		tls_allocator.arg = altcp_tls_create_config_client(NULL, 0);

		if (!tls_allocator.arg) {
			log_err("error: tls_allocator arg is null\n");
			return -1;
		}

		conn.altcp_allocator = &tls_allocator;
	}

#if defined(CONFIG_LWIP_DEBUG)
	mbedtls_debug_set_threshold(99);
#endif

	if (httpc_get_file_dns(server_name, port, path, &conn, httpc_recv_cb,
				   &ctx, &state)) {
		net_lwip_remove_netif(netif);
		return CMD_RET_FAILURE;
	}

	while (!ctx.done) {
		net_lwip_rx(udev, netif);
		sys_check_timeouts();
		if (ctrlc())
			break;
	}

	net_lwip_remove_netif(netif);

	if (ctx.done == SUCCESS)
		return 0;

	return -1;
}

int wget_with_dns(ulong dst_addr, char *uri)
{
	eth_set_current();

	return wget_loop(eth_get_dev(), dst_addr, uri);
}

int do_wget(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	char *end;
	char *url;
	ulong dst_addr;

	if (argc < 2 || argc > 3)
		return CMD_RET_USAGE;

	dst_addr = hextoul(argv[1], &end);
	if (end == (argv[1] + strlen(argv[1]))) {
		if (argc < 3)
			return CMD_RET_USAGE;
		url = argv[2];
	} else {
		dst_addr = image_load_addr;
		url = argv[1];
	}

	if (wget_with_dns(dst_addr, url))
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}
