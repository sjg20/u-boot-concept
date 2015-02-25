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
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>

int sandbox_eth_raw_os_init(const char *ifname, unsigned char *ethmac,
			    struct eth_sandbox_raw_priv *priv)
{
	if (priv->local) {
		struct sockaddr_in *device;
		int ret;
		struct timeval tv;
		int one = 1;

		/* Prepare device struct */
		priv->device = malloc(sizeof(struct sockaddr_in));
		device = priv->device;
		memset(device, 0, sizeof(struct sockaddr_in));
		device->sin_family = AF_INET;
		ret = inet_pton(AF_INET, "127.0.0.1",
			  (struct in_addr *)&device->sin_addr.s_addr);
		if (ret < 0) {
			printf("Failed to convert address: %d %s\n", errno,
			       strerror(errno));
			return -errno;
		}

		/**
		 * Open socket
		 *  Since we specify UDP here, any incoming ICMP packets will
		 *  not be received, so things like ping will not work on this
		 *  localhost interface.
		 */
		priv->sd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
		if (priv->sd < 0) {
			printf("Failed to open socket: %d %s\n", errno,
			       strerror(errno));
			return -errno;
		}

		/* Allow the receive to timeout after a millisecond */
		tv.tv_sec = 0;
		tv.tv_usec = 1000;
		ret = setsockopt(priv->sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,
			   sizeof(struct timeval));
		if (ret < 0) {
			printf("Failed to set opt: %d %s\n", errno,
			       strerror(errno));
			return -errno;
		}

		/* Include the UDP/IP headers on send and receive */
		ret = setsockopt(priv->sd, IPPROTO_IP, IP_HDRINCL, &one,
				 sizeof(one));
		if (ret < 0) {
			printf("Failed to set opt: %d %s\n", errno,
			       strerror(errno));
			return -errno;
		}
		priv->local_bind_sd = -1;
		priv->local_bind_udp_port = 0;
	} else {
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
	}

	return 0;
}

int sandbox_eth_raw_os_send(void *packet, int length,
			    struct eth_sandbox_raw_priv *priv)
{
	int retval;
	struct udphdr *udph = packet + sizeof(struct iphdr);

	if (!priv->sd || !priv->device)
		return -EINVAL;

	if (priv->local && (priv->local_bind_sd == -1 ||
			    priv->local_bind_udp_port != udph->source)) {
		struct iphdr *iph = packet;
		struct sockaddr_in addr;

		if (priv->local_bind_sd != -1)
			close(priv->local_bind_sd);

		/* A normal UDP socket is required to bind */
		priv->local_bind_sd = socket(AF_INET, SOCK_DGRAM, 0);
		if (priv->local_bind_sd < 0) {
			printf("Failed to open bind sd: %d %s\n", errno,
			       strerror(errno));
			return -errno;
		}
		priv->local_bind_udp_port = udph->source;

		/**
		 * Bind the UDP port that we intend to use as our source port
		 * so that the kernel will not send ICMP port unreachable
		 */
		addr.sin_family = AF_INET;
		addr.sin_port = udph->source;
		addr.sin_addr.s_addr = iph->saddr;
		retval = bind(priv->local_bind_sd, &addr, sizeof(addr));
		if (retval < 0)
			printf("Failed to bind: %d %s\n", errno,
			       strerror(errno));
	}

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
	if (priv->local) {
		if (priv->local_bind_sd != -1)
			close(priv->local_bind_sd);
		priv->local_bind_sd = -1;
		priv->local_bind_udp_port = 0;
	}
}
