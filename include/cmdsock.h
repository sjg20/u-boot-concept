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
 * @started: true if U-Boot ha already run the init sequence
 * @in: Input buffer, for traffic from the client
 * @out: Output buffer, for traffic to the client
 */
struct cmdsock {
	bool have_err;
	bool inited;
	struct membuf *in;
	struct membuf *out;
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
 */
int cmdsock_poll(struct membuf *in, struct membuf *out);

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
 * cmdsock_init() - Set up a cmdsock
 *
 * Registers the input and output buffers to use with the cmdsock
 *
 * @in: Input buffer, for traffic from the client
 * @out: Output buffer, for traffic to the client
 */
void cmdsock_init(struct membuf *in, struct membuf *out);

/**
 * cmdsock_process() - Check for available commands and process them
 *
 * Return: 0 (always)
 */
int cmdsock_process(void);

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
 * Return: 0 on success (always)
 */
int cmdsock_puts(const char *s);

#ifdef CONFIG_CMDSOCK

/**
 * cmdsock_active() - Check if a cmdsock is active
 *
 * Return: true if the feature is enabled and a socket is being used, else false
 */
bool cmdsock_active(void);

#else

static inline bool cmdsock_active(void)
{
	return false;
}
#endif

#endif
