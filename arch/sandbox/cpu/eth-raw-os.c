/*
 * Copyright (c) 2015 National Instruments
 *
 * (C) Copyright 2015
 * Joe Hershberger <joe.hershberger@ni.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <asm/eth-raw-os.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <linux/if_ether.h>
#include <linux/if_packet.h>

int sandbox_eth_raw_os_init(const char *ifname, unsigned char *ethmac,
			    struct eth_sandbox_raw_priv *priv)
{
	struct sockaddr_ll *device;
	struct packet_mreq mr;

	/* Prepare device struct */
	priv->device = malloc(sizeof(struct sockaddr_ll));
	device = priv->device;
	memset(device, 0, sizeof(struct sockaddr_ll));
	device->sll_ifindex = if_nametoindex(ifname);
	device->sll_family = AF_PACKET;
	memcpy(device->sll_addr, ethmac, 6);
	device->sll_halen = htons(6);

	/* Open socket */
	priv->sd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (priv->sd < 0) {
		printf("Failed to open socket: %d %s\n", errno,
		       strerror(errno));
		return -errno;
	}
	/* Bind to the specified interface */
	setsockopt(priv->sd, SOL_SOCKET, SO_BINDTODEVICE, ifname,
		   strlen(ifname) + 1);

	/* Enable promiscuous mode to receive responses meant for us */
	mr.mr_ifindex = device->sll_ifindex;
	mr.mr_type = PACKET_MR_PROMISC;
	setsockopt(priv->sd, SOL_PACKET, PACKET_ADD_MEMBERSHIP,
		   &mr, sizeof(mr));
	return 0;
}

int sandbox_eth_raw_os_send(void *packet, int length,
			    const struct eth_sandbox_raw_priv *priv)
{
	int retval;

	if (!priv->sd || !priv->device)
		return -EINVAL;

	retval = sendto(priv->sd, packet, length, 0,
			(struct sockaddr *)priv->device,
			sizeof(struct sockaddr_ll));
	if (retval < 0)
		printf("Failed to send packet: %d %s\n", errno,
		       strerror(errno));
	return retval;
}

int sandbox_eth_raw_os_recv(void *packet, int *length,
			    const struct eth_sandbox_raw_priv *priv)
{
	int retval;
	int saddr_size;

	if (!priv->sd || !priv->device)
		return -EINVAL;
	saddr_size = sizeof(struct sockaddr);
	retval = recvfrom(priv->sd, packet, 1536, 0,
			  (struct sockaddr *)priv->device,
			  (socklen_t *)&saddr_size);
	*length = 0;
	if (retval > 0) {
		*length = retval;
		return 0;
	}
	return retval;
}

void sandbox_eth_raw_os_halt(struct eth_sandbox_raw_priv *priv)
{
	free((struct sockaddr_ll *)priv->device);
	priv->device = NULL;
	close(priv->sd);
	priv->sd = -1;
}
