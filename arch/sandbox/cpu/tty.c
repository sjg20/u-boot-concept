// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2024 U-Boot TKey Support
 *
 * TTY configuration for TKey serial communication
 */

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <asm/termbits.h>
#include <sys/ioctl.h>
#include <linux/serial.h>

int os_tty_set_params(int fd)
{
	struct termios2 tty2;

	/* Get current termios2 attributes */
	if (ioctl(fd, TCGETS2, &tty2) != 0)
		return -errno;

	/* Configure for raw mode */
	tty2.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	tty2.c_oflag &= ~OPOST;
	tty2.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	tty2.c_cflag &= ~(CSIZE | PARENB);

	/* 8N1 configuration */
	tty2.c_cflag |= CS8;                /* 8 data bits */
	tty2.c_cflag &= ~PARENB;            /* No parity */
	tty2.c_cflag &= ~CSTOPB;            /* 1 stop bit */
	tty2.c_cflag |= (CLOCAL | CREAD);   /* Enable receiver, ignore modem lines */

	/* Set custom baud rate using termios2 */
	tty2.c_cflag &= ~CBAUD;
	tty2.c_cflag |= BOTHER;             /* Use custom baud rate */
	tty2.c_ispeed = 62500;              /* Input speed */
	tty2.c_ospeed = 62500;              /* Output speed */

	/* Blocking with timeout for complete frames */
	tty2.c_cc[VMIN] = 1;   /* Wait for at least 1 character */
	tty2.c_cc[VTIME] = 50; /* 5 second timeout */

	/* Apply termios2 settings */
	if (ioctl(fd, TCSETS2, &tty2) != 0)
		return -errno;

	/* Flush buffers */
	if (ioctl(fd, TCFLSH, TCIOFLUSH) != 0)
		return -errno;

	return 0;
}
