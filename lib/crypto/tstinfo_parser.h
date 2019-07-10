/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * TSTInfo Parser
 *
 * Copyright (c) 2019 Linaro Limited
 *			Author: AKASHI Takahiro
 */

#ifndef _TSTINFO_PARSER_H
#define _TSTINFO_PARSER_H

#include <crypto/tstinfo.h>
#include <linux/oid_registry.h>
#include <linux/time.h>

struct extension {
	struct extension *next;
	enum OID oid;
	bool critical;
	const void *data;
	size_t size;
};

struct tstinfo {
	u8 version;
	enum OID policy;
	struct {
		enum OID algo;
		const void *data;
		size_t size;
	} digest;
	u64 serial_hi; /* up to 160bits */
	u64 serial_lo;
	time64_t time;
	struct {
		int sec;
		int msec;
		int usec;
	} accuracy;
	struct {
		const void *data;
		size_t size;
	} tsa;
	struct extension *ext_next;
};
#endif /* _TSTINFO_PARSER_H */
