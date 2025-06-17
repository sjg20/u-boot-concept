/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Command-socket interface
 *
 * Provides a way to communicate with sandbox from another process. U-Boot
 * becomes a server, with its features made available in a primitive way over a
 * unix-domain socket.
 *
 * This is only intended to support a single client...
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#ifndef __CMDSOCK_H__
#define __CMDSOCK_H__

#include <stdbool.h>

struct membuf;

/* Maximum size of the i/o network buffers within sandbox */
#define CMDSOCK_BUF_SIZE	65536

/**
 * struct cmdsock - information about the cmdsock interface
 *
 * @have_err: 0 if OK, -ve if there is an error code pending which needs to be
 *	sent to the client
 * @started: true if U-Boot has already run the init sequence
 * @capture: true to send U-Boot stdout over the cmdsock
 * @in: Input buffer, for traffic from the client
 * @out: Output buffer, for traffic to the client
 */
struct cmdsock {
	bool have_err;
	bool inited;
	bool capture;
	struct membuf *in;
	struct membuf *out;
};

enum cmdsock_poll_t {
	CMDSOCKPR_OK,
	CMDSOCKPR_LISTEN_ERR,
	CMDSOCKPR_ACCEPT_ERR,
	CMDSOCKPR_SELECT_ERR,
	CMDSOCKPR_NEW_CLIENT,
	CMDSOCKPR_DISCONNECT,
};

/**
 * cmdsock_poll() - Poll the command socket
 *
 * Accepts connections and transfers data in and out.
 *
 * This is an internal function.
 *
 * @in: Input buffer, for traffic from the client
 * @out: Output buffer, for traffic to the client
 * Return: Result of poll, or -ve on error
 */
enum cmdsock_poll_t cmdsock_poll(struct membuf *in, struct membuf *out);

/**
 * cmdsock_start() - Start the command socket
 *
 * Starts listening for connections on theee given socket
 *
 * This is an internal function.
 *
 * @path: File path of the unix-domain socket to use
 * Return: 0 if OK, -ve on error
 */
int cmdsock_start(const char *path);

/**
 * cmdsock_stop() - Stop the command socket
 *
 * This is an internal function.
 *
 * Closes the socket; the client will be disconnected
 */
void cmdsock_stop(void);

/**
 * sandbox_cmdsock_loop() - Sandbox implementation of the command-socket loop
 *
 * If a cmdsock is in use, this sets it up and loops waiting for clients and
 * requests. This function does not return until the socket is closed, e.g.
 * due to an error, or an external request.
 */
void sandbox_cmdsock_loop(void);

/**
 * cmdsock_run() - Set up and run a cmdsock
 *
 * Registers the input and output buffers to use with the cmdsock. Runs the
 * cmdsock poll loop until done
 *
 * @in: Input buffer, for traffic from the client
 * @out: Output buffer, for traffic to the client
 */
void cmdsock_run(struct membuf *in, struct membuf *out);

/**
 * cmdsock_process() - Check for available commands and process them
 *
 * @status: Return value from last call to cmdsock_poll()
 * Return: 0 (always)
 */
int cmdsock_process(enum cmdsock_poll_t status);

/**
 * cmdsock_putc() - Handle writing a character
 *
 * If a client is connected, this sends a message with the character.
 *
 * This must only be called if cmdsock_active()
 *
 * @ch: Character to send
 * Return: 0 on success (always)
 */
int cmdsock_putc(int ch);

/**
 * cmdsock_puts() - Handle writing a string
 *
 * If a client is connected, this sends a message with the string.
 *
 * This must only be called if cmdsock_active()
 *
 * @str: String to send
 * @len: Number of bytes of string to send
 * Return: 0 on success (always)
 */
int cmdsock_puts(const char *s, int len);

#ifdef CONFIG_CMDSOCK

/**
 * cmdsock_active() - Check if the cmdsock feature active
 *
 * Return: true if the feature is enabled, else false
 */
bool cmdsock_active(void);

/**
 * cmdsock_connected() - Check if a cmdsock is connected
 *
 * Return: true if cmdsock socket is connected
 */
bool cmdsock_connected(void);

#else

static inline bool cmdsock_active(void)
{
	return false;
}

static inline bool cmdsock_connected(void)
{
	return false;
}

#endif

#endif
