/*
 * From Coreboot ip_checksum.h
 *
 * Copyright (c) 2014 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef _IP_CHECKSUM_H
#define _IP_CHECKSUM_H

unsigned long compute_ip_checksum(void *addr, unsigned long length);
unsigned long add_ip_checksums(unsigned long offset, unsigned long sum,
			       unsigned long new);

#endif /* IP_CHECKSUM_H */
